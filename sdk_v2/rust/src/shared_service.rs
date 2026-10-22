//! SDK-controlled sharing of a Foundry serving process across independent clients.
//!
//! Catalog and acquisition remain ordinary SDK operations. Only serving runs in
//! `foundry-local-host`; dropping a client never unloads models or stops that host.
//! The state directory must be private to the current user. Inference retains the
//! existing loopback HTTP server's access policy; loopback is not authentication.

use std::collections::BTreeMap;
use std::fs::{self, File, OpenOptions};
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::time::{Duration, Instant};

use fs2::FileExt;
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};

use crate::error::{FoundryLocalError, Result};
use crate::CatalogType;

const PROTOCOL_VERSION: u32 = 1;
const START_TIMEOUT: Duration = Duration::from_secs(120);

/// A single shared serving configuration. Different configurations must use
/// different state directories; an incompatible live host is never replaced.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct SharedServiceConfig {
    /// User-private directory containing coordination files, not model weights.
    pub directory: PathBuf,
    /// Packaged `foundry-local-host` executable matching this SDK.
    pub executable: PathBuf,
    /// Native Foundry library file used by both catalog client and host.
    pub library: PathBuf,
    /// Shared model cache; downloads use the existing SDK.
    pub model_cache_dir: PathBuf,
    /// Shared SDK application data, including local model registrations.
    pub app_data_dir: PathBuf,
    /// Execution providers to prepare/register in the serving process through
    /// existing SDK acquisition. Empty means no additional provider preparation.
    #[serde(default)]
    pub execution_providers: Vec<String>,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
struct HostIdentity {
    protocol: u32,
    sdk_version: String,
    executable_hash: String,
    library_hash: String,
    dependency_hashes: BTreeMap<String, String>,
    config: SharedServiceConfig,
}

#[derive(Serialize, Deserialize)]
struct HostRecord {
    identity: HostIdentity,
    pid: u32,
    endpoint: String,
}

/// Lightweight SDK control client. Construction does not start a process or load
/// a native library. Call catalog APIs separately for discovery and preparation.
pub struct SharedService {
    config: SharedServiceConfig,
}

/// Observation of a compatible live host, not a model residency reservation.
#[derive(Clone)]
pub struct SharedServiceConnection {
    endpoint: String,
    client: reqwest::Client,
}

/// Exact serving observation returned by a successful load. Context length comes
/// from the loaded native configuration, not the catalog; absence means unknown.
#[derive(Clone, Debug, Deserialize, Serialize)]
pub struct SharedModelState {
    /// Resolved model variant ID in the selected catalog.
    pub model_id: String,
    /// Loaded native context window, when reported by the serving runtime.
    pub context_length: Option<u64>,
}

impl SharedService {
    /// Configure shared serving without starting it.
    pub fn new(config: SharedServiceConfig) -> Result<Self> {
        if !config.directory.is_absolute()
            || !config.executable.is_absolute()
            || !config.library.is_absolute()
            || !config.model_cache_dir.is_absolute()
            || !config.app_data_dir.is_absolute()
        {
            return Err(invalid("shared service paths must be absolute"));
        }
        if config
            .execution_providers
            .iter()
            .any(|name| name.trim().is_empty())
        {
            return Err(invalid("execution-provider names must not be empty"));
        }
        Ok(Self { config })
    }

    /// Observe a running compatible host, without starting one. Errors such as
    /// incompatible identity or unreachable HTTP are not reported as absence.
    pub async fn connect(&self) -> Result<Option<SharedServiceConnection>> {
        let config = self.config.clone();
        let record = crate::detail::task::spawn_blocking(move || {
            if !config.directory.exists() {
                return Ok(None);
            }
            let identity = identity(config)?;
            read_live_record(&identity)
        })
        .await?;
        match record {
            Some(record) => Ok(Some(connect_record(record).await?)),
            None => Ok(None),
        }
    }

    /// Start or reuse the compatible host. Concurrent clients serialize startup,
    /// while the host owns a separate lifetime lock. Cancelling this waiter does
    /// not kill a process or cancel another client's work.
    pub async fn ensure_started(&self) -> Result<SharedServiceConnection> {
        let config = self.config.clone();
        let identity = crate::detail::task::spawn_blocking(move || {
            create_private_directory(&config.directory)?;
            fs::create_dir_all(&config.model_cache_dir)?;
            fs::create_dir_all(&config.app_data_dir)?;
            identity(config)
        })
        .await?;
        let start = Instant::now();
        let startup_lock = open_lock(&identity.config.directory.join("startup.lock"))?;
        loop {
            match startup_lock.try_lock_exclusive() {
                Ok(()) => break,
                Err(error) if lock_contended(&error) => {
                    check_deadline(start)?;
                    tokio::time::sleep(Duration::from_millis(50)).await;
                }
                Err(error) => return Err(error.into()),
            }
        }

        if let Some(record) = read_live_record(&identity)? {
            return connect_record(record).await;
        }

        // The start lock protects publication of the host configuration. A host
        // still starting after its initiating client died owns host.lock; don't
        // launch or overwrite its configuration.
        let host_lock = open_lock(&identity.config.directory.join("host.lock"))?;
        let already_starting = match host_lock.try_lock_exclusive() {
            Ok(()) => false,
            Err(error) if lock_contended(&error) => true,
            Err(error) => return Err(error.into()),
        };
        let mut process_exit = None;
        if !already_starting {
            let config_path = identity.config.directory.join("config.json");
            fs::write(&config_path, serde_json::to_vec(&identity)?)?;
            let _ = fs::remove_file(identity.config.directory.join("ready.json"));
            FileExt::unlock(&host_lock)?;
            let mut command = Command::new(&identity.config.executable);
            command
                .arg("--config")
                .arg(config_path)
                .stdin(Stdio::null())
                .stdout(Stdio::null())
                .stderr(Stdio::from(
                    OpenOptions::new()
                        .create(true)
                        .append(true)
                        .open(identity.config.directory.join("host.log"))?,
                ));
            #[cfg(windows)]
            {
                use std::os::windows::process::CommandExt;
                // The serving process must survive the initiating terminal.
                command.creation_flags(0x00000008 | 0x08000000);
            }
            #[cfg(unix)]
            {
                use std::os::unix::process::CommandExt;
                command.process_group(0);
            }
            let mut child = command.spawn()?;
            let (sender, receiver) = tokio::sync::oneshot::channel();
            std::thread::spawn(move || {
                let _ = sender.send(child.wait());
            });
            process_exit = Some(receiver);
        }
        drop(host_lock);

        loop {
            if let Some(record) = read_live_record(&identity)? {
                return connect_record(record).await;
            }
            if let Some(receiver) = &mut process_exit {
                match receiver.try_recv() {
                    Ok(status) => {
                        return Err(FoundryLocalError::ModelOperation {
                            reason: format!("shared host exited ({:?}); inspect host.log", status?),
                        });
                    }
                    Err(tokio::sync::oneshot::error::TryRecvError::Empty) => {}
                    Err(error) => {
                        return Err(invalid(format!("shared host monitor failed: {error}")))
                    }
                }
            }
            check_deadline(start)?;
            tokio::time::sleep(Duration::from_millis(50)).await;
        }
    }
}

impl SharedServiceConnection {
    /// Base OpenAI-compatible inference endpoint for the selected catalog.
    pub fn inference_endpoint(&self, catalog: CatalogType) -> String {
        format!("{}{}/v1", self.endpoint, catalog_prefix(catalog))
    }

    /// Load an exact catalog variant in the serving process. This does not pin
    /// residency after completion or reserve resources for a later HTTP request.
    pub async fn load(&self, catalog: CatalogType, model_id: &str) -> Result<SharedModelState> {
        let state: SharedModelState =
            serde_json::from_value(self.operation(catalog, "load", model_id).await?)?;
        if state.model_id != model_id || state.context_length == Some(0) {
            return Err(invalid(
                "shared host returned a different model ID or invalid loaded context",
            ));
        }
        Ok(state)
    }

    /// Explicit shared unload. An in-use model is rejected by native protection;
    /// failure is never converted to a deferred unload.
    pub async fn unload(&self, catalog: CatalogType, model_id: &str) -> Result<()> {
        self.operation(catalog, "unload", model_id)
            .await
            .map(|_| ())
    }

    /// Query this server's loaded IDs, not the calling process's native manager.
    pub async fn loaded_models(&self, catalog: CatalogType) -> Result<Vec<String>> {
        Ok(checked(
            self.client
                .get(format!(
                    "{}{}/models/loaded",
                    self.endpoint,
                    catalog_prefix(catalog)
                ))
                .send()
                .await?,
        )
        .await?
        .json()
        .await?)
    }

    async fn operation(
        &self,
        catalog: CatalogType,
        operation: &str,
        model_id: &str,
    ) -> Result<serde_json::Value> {
        if model_id.is_empty() {
            return Err(invalid("model ID must not be empty"));
        }
        Ok(checked(
            self.client
                .get(format!(
                    "{}{}/models/{operation}/{}",
                    self.endpoint,
                    catalog_prefix(catalog),
                    urlencoding::encode(model_id)
                ))
                .send()
                .await?,
        )
        .await?
        .json()
        .await?)
    }
}

fn catalog_prefix(catalog: CatalogType) -> &'static str {
    match catalog {
        CatalogType::Public => "",
        CatalogType::Local => "/catalogs/local",
    }
}

fn invalid(message: impl Into<String>) -> FoundryLocalError {
    FoundryLocalError::InvalidConfiguration {
        reason: message.into(),
    }
}

fn identity(mut config: SharedServiceConfig) -> Result<HostIdentity> {
    config.execution_providers.sort();
    config.execution_providers.dedup();
    config.executable = fs::canonicalize(&config.executable)?;
    config.library = fs::canonicalize(&config.library)?;
    config.directory = fs::canonicalize(&config.directory)?;
    config.model_cache_dir = fs::canonicalize(&config.model_cache_dir)?;
    config.app_data_dir = fs::canonicalize(&config.app_data_dir)?;
    let mut dependency_hashes = BTreeMap::new();
    for entry in fs::read_dir(
        config
            .library
            .parent()
            .ok_or_else(|| invalid("native library has no directory"))?,
    )? {
        let path = entry?.path();
        let name = path
            .file_name()
            .and_then(|name| name.to_str())
            .unwrap_or_default();
        let is_library = if cfg!(windows) {
            name.to_ascii_lowercase().ends_with(".dll")
        } else if cfg!(target_os = "macos") {
            name.ends_with(".dylib")
        } else {
            name.ends_with(".so") || name.contains(".so.")
        };
        if path != config.library && is_library && path.metadata()?.is_file() {
            dependency_hashes.insert(name.to_owned(), file_hash(&path)?);
        }
    }
    Ok(HostIdentity {
        protocol: PROTOCOL_VERSION,
        sdk_version: env!("CARGO_PKG_VERSION").into(),
        executable_hash: file_hash(&config.executable)?,
        library_hash: file_hash(&config.library)?,
        dependency_hashes,
        config,
    })
}

fn file_hash(path: &Path) -> Result<String> {
    use std::io::Read;
    let mut file = File::open(path)?;
    let mut hash = Sha256::new();
    let mut buffer = [0; 65536];
    loop {
        let count = file.read(&mut buffer)?;
        if count == 0 {
            break;
        }
        hash.update(&buffer[..count]);
    }
    Ok(format!("{:x}", hash.finalize()))
}

fn open_lock(path: &Path) -> Result<File> {
    Ok(OpenOptions::new()
        .read(true)
        .write(true)
        .create(true)
        .truncate(false)
        .open(path)?)
}

fn read_live_record(expected: &HostIdentity) -> Result<Option<HostRecord>> {
    let lock = open_lock(&expected.config.directory.join("host.lock"))?;
    match lock.try_lock_exclusive() {
        Ok(()) => return Ok(None),
        Err(error) if lock_contended(&error) => {}
        Err(error) => return Err(error.into()),
    }
    let data = match fs::read(expected.config.directory.join("ready.json")) {
        Ok(data) => data,
        Err(error) if error.kind() == std::io::ErrorKind::NotFound => return Ok(None),
        Err(error) => return Err(error.into()),
    };
    let record: HostRecord = serde_json::from_slice(&data)?;
    if &record.identity != expected {
        return Err(invalid(
            "a different shared host configuration is running; it was not replaced",
        ));
    }
    let url = reqwest::Url::parse(&record.endpoint).map_err(|error| invalid(error.to_string()))?;
    if url.scheme() != "http" || url.host_str() != Some("127.0.0.1") || url.port().is_none() {
        return Err(invalid(
            "shared host must advertise an explicit IPv4 loopback HTTP endpoint",
        ));
    }
    Ok(Some(record))
}

async fn connect_record(record: HostRecord) -> Result<SharedServiceConnection> {
    let client = reqwest::Client::builder()
        .no_proxy()
        .redirect(reqwest::redirect::Policy::none())
        .connect_timeout(Duration::from_secs(5))
        .timeout(Duration::from_secs(300))
        .build()?;
    let status: serde_json::Value = checked(
        client
            .get(format!("{}/status", record.endpoint))
            .timeout(Duration::from_secs(5))
            .send()
            .await?,
    )
    .await?
    .json()
    .await?;
    if status.get("pid").and_then(serde_json::Value::as_u64) != Some(u64::from(record.pid)) {
        return Err(invalid(
            "shared serving endpoint no longer belongs to the recorded host",
        ));
    }
    Ok(SharedServiceConnection {
        endpoint: record.endpoint,
        client,
    })
}

async fn checked(response: reqwest::Response) -> Result<reqwest::Response> {
    if !response.status().is_success() {
        return Err(FoundryLocalError::ModelOperation {
            reason: format!(
                "shared host returned {}: {}",
                response.status(),
                response.text().await?
            ),
        });
    }
    Ok(response)
}

fn check_deadline(start: Instant) -> Result<()> {
    if start.elapsed() >= START_TIMEOUT {
        return Err(FoundryLocalError::ModelOperation {
            reason: "shared host startup did not complete; inspect host.log and reconcile state before retrying".into(),
        });
    }
    Ok(())
}

fn lock_contended(error: &std::io::Error) -> bool {
    error.raw_os_error() == fs2::lock_contended_error().raw_os_error()
}

fn create_private_directory(path: &Path) -> Result<()> {
    #[cfg(unix)]
    {
        use std::os::unix::fs::DirBuilderExt;
        fs::DirBuilder::new()
            .recursive(true)
            .mode(0o700)
            .create(path)?;
        use std::os::unix::fs::PermissionsExt;
        if fs::metadata(path)?.permissions().mode() & 0o077 != 0 {
            return Err(invalid(
                "shared service directory must be private to the current user",
            ));
        }
    }
    #[cfg(not(unix))]
    fs::create_dir_all(path)?;
    Ok(())
}

/// Entry point for the packaged host executable, not an application lifecycle
/// API. Clients should use [`SharedService`] rather than invoking this directly.
#[doc(hidden)]
pub async fn run_host(config_path: &Path) -> Result<()> {
    let requested: HostIdentity = serde_json::from_slice(&fs::read(config_path)?)?;
    let actual = identity(requested.config.clone())?;
    if requested != actual
        || fs::canonicalize(std::env::current_exe()?)? != actual.config.executable
    {
        return Err(invalid("shared host artifacts changed before startup"));
    }
    let lock = open_lock(&actual.config.directory.join("host.lock"))?;
    lock.try_lock_exclusive()?;
    let config = crate::FoundryLocalConfig::new("foundry-shared-service")
        .library_path(native_path(&actual.config.library)?)
        .model_cache_dir(native_path(&actual.config.model_cache_dir)?)
        .app_data_dir(native_path(&actual.config.app_data_dir)?)
        .logs_dir(native_path(&actual.config.app_data_dir.join("logs"))?)
        .web_service_urls("http://127.0.0.1:0");
    let manager = crate::FoundryLocalManager::create(config)?;
    if !actual.config.execution_providers.is_empty() {
        let names: Vec<&str> = actual
            .config
            .execution_providers
            .iter()
            .map(String::as_str)
            .collect();
        let result = manager.download_and_register_eps(Some(&names)).await?;
        if !result.success {
            return Err(FoundryLocalError::ModelOperation {
                reason: format!(
                    "shared host execution-provider preparation failed: {}",
                    result.status
                ),
            });
        }
    }
    manager.start_web_service().await?;
    let endpoint = manager
        .urls()?
        .into_iter()
        .next()
        .ok_or_else(|| invalid("host has no endpoint"))?;
    let record = HostRecord {
        identity: actual,
        pid: std::process::id(),
        endpoint,
    };
    let ready_path = record.identity.config.directory.join("ready.json");
    let pending_path = ready_path.with_extension("pending");
    fs::write(&pending_path, serde_json::to_vec(&record)?)?;
    fs::rename(pending_path, &ready_path)?;
    // A normal client release does not stop the host. The process owns all native
    // objects and remains alive until explicit host shutdown or process exit.
    while !manager.is_shutdown_requested() {
        tokio::time::sleep(Duration::from_secs(1)).await;
    }
    manager.shutdown()?;
    fs::remove_file(ready_path)?;
    Ok(())
}

fn native_path(path: &Path) -> Result<String> {
    let value = path
        .to_str()
        .ok_or_else(|| invalid("native paths must be valid UTF-8"))?;
    // Native configuration still joins some paths with '/', which Windows
    // extended-length paths prohibit. Keep canonical identity, but pass ordinary
    // drive/UNC syntax across this existing UTF-8 SDK boundary.
    #[cfg(windows)]
    {
        if let Some(unc) = value.strip_prefix(r"\\?\UNC\") {
            return Ok(format!(r"\\{unc}"));
        }
        if let Some(drive) = value.strip_prefix(r"\\?\") {
            return Ok(drive.to_owned());
        }
    }
    Ok(value.to_owned())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::{Read, Write};

    fn fixture() -> (tempfile::TempDir, HostIdentity) {
        let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("../cpp/build/shared-serving/tests");
        fs::create_dir_all(&root).unwrap();
        let dir = tempfile::tempdir_in(root).unwrap();
        let config = SharedServiceConfig {
            directory: dir.path().join("state"),
            executable: dir.path().join("host"),
            library: dir.path().join("native"),
            model_cache_dir: dir.path().join("models"),
            app_data_dir: dir.path().join("data"),
            execution_providers: Vec::new(),
        };
        create_private_directory(&config.directory).unwrap();
        fs::create_dir_all(&config.model_cache_dir).unwrap();
        fs::create_dir_all(&config.app_data_dir).unwrap();
        fs::write(&config.executable, "test host").unwrap();
        fs::write(&config.library, "test native").unwrap();
        let identity = identity(config).unwrap();
        (dir, identity)
    }

    #[test]
    fn stale_record_is_absent_and_incompatible_live_host_is_not_adopted() {
        let (_dir, identity) = fixture();
        let ready = identity.config.directory.join("ready.json");
        let record = HostRecord {
            identity: identity.clone(),
            pid: 123,
            endpoint: "http://127.0.0.1:12345".into(),
        };
        fs::write(&ready, serde_json::to_vec(&record).unwrap()).unwrap();
        assert!(read_live_record(&identity).unwrap().is_none());
        let lock = open_lock(&identity.config.directory.join("host.lock")).unwrap();
        lock.lock_exclusive().unwrap();
        assert!(read_live_record(&identity).unwrap().is_some());
        let mut changed = identity.clone();
        changed.sdk_version = "different".into();
        assert!(read_live_record(&changed).is_err());
        assert!(ready.exists(), "incompatible host must not be removed");
    }

    #[test]
    fn native_replacement_changes_compatibility() {
        let (_dir, old) = fixture();
        fs::write(&old.config.library, "replaced native").unwrap();
        let updated = identity(old.config.clone()).unwrap();
        assert_ne!(old, updated);
    }

    #[test]
    fn adjacent_runtime_replacement_changes_compatibility() {
        let (_dir, identity_before) = fixture();
        let name = if cfg!(windows) {
            "onnxruntime.dll"
        } else if cfg!(target_os = "macos") {
            "libonnxruntime.dylib"
        } else {
            "libonnxruntime.so"
        };
        let dependency = identity_before.config.library.with_file_name(name);
        fs::write(&dependency, "runtime one").unwrap();
        let before = identity(identity_before.config.clone()).unwrap();
        fs::write(&dependency, "runtime two").unwrap();
        let after = identity(identity_before.config).unwrap();
        assert_ne!(before, after);
    }

    fn http_reply(status: &str, body: &str) -> (String, std::thread::JoinHandle<String>) {
        let listener = std::net::TcpListener::bind("127.0.0.1:0").unwrap();
        let endpoint = format!("http://{}", listener.local_addr().unwrap());
        let response = format!(
            "HTTP/1.1 {status}\r\nContent-Type: application/json\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{body}",
            body.len()
        );
        let thread = std::thread::spawn(move || {
            let (mut socket, _) = listener.accept().unwrap();
            socket
                .set_read_timeout(Some(Duration::from_secs(5)))
                .unwrap();
            let mut request = Vec::new();
            let mut byte = [0];
            while !request.ends_with(b"\r\n\r\n") {
                assert_eq!(socket.read(&mut byte).unwrap(), 1);
                request.push(byte[0]);
            }
            socket.write_all(response.as_bytes()).unwrap();
            String::from_utf8(request).unwrap()
        });
        (endpoint, thread)
    }

    #[tokio::test]
    async fn load_routes_exact_local_id_and_busy_unload_is_an_error() {
        let (endpoint, server) = http_reply(
            "200 OK",
            r#"{"status":"loaded","model_id":"coding:1","context_length":4096}"#,
        );
        let connection = SharedServiceConnection {
            endpoint,
            client: reqwest::Client::builder().no_proxy().build().unwrap(),
        };
        let state = connection
            .load(CatalogType::Local, "coding:1")
            .await
            .unwrap();
        assert_eq!(state.context_length, Some(4096));
        assert!(server
            .join()
            .unwrap()
            .starts_with("GET /catalogs/local/models/load/coding%3A1 "));
        let (endpoint, server) = http_reply("409 Conflict", r#"{"error":"model in use"}"#);
        let connection = SharedServiceConnection {
            endpoint,
            ..connection
        };
        let error = connection
            .unload(CatalogType::Public, "coding:1")
            .await
            .unwrap_err();
        assert!(error.to_string().contains("409"), "{error}");
        assert!(server
            .join()
            .unwrap()
            .starts_with("GET /models/unload/coding%3A1 "));
    }

    #[tokio::test]
    async fn malformed_loaded_state_is_not_an_empty_success() {
        let (endpoint, server) = http_reply("200 OK", "{}");
        let connection = SharedServiceConnection {
            endpoint,
            client: reqwest::Client::builder().no_proxy().build().unwrap(),
        };
        assert!(connection.loaded_models(CatalogType::Public).await.is_err());
        server.join().unwrap();
    }

    #[tokio::test]
    async fn load_rejects_wrong_identity_invalid_context_and_redirects() {
        for (status, body) in [
            (
                "200 OK",
                r#"{"model_id":"different:1","context_length":4096}"#,
            ),
            ("200 OK", r#"{"model_id":"coding:1","context_length":0}"#),
            (
                "302 Found",
                r#"{"model_id":"coding:1","context_length":4096}"#,
            ),
        ] {
            let (endpoint, server) = http_reply(status, body);
            let connection = SharedServiceConnection {
                endpoint,
                client: reqwest::Client::builder().no_proxy().build().unwrap(),
            };
            assert!(connection
                .load(CatalogType::Public, "coding:1")
                .await
                .is_err());
            server.join().unwrap();
        }
    }

    #[tokio::test]
    async fn ready_endpoint_must_belong_to_recorded_process() {
        let (_dir, identity) = fixture();
        let (endpoint, server) = http_reply("200 OK", r#"{"pid":456}"#);
        let record = HostRecord {
            identity,
            endpoint,
            pid: 123,
        };
        assert!(connect_record(record).await.is_err());
        server.join().unwrap();
    }
}
