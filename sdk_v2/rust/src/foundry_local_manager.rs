//! Top-level entry point for the Foundry Local SDK.
//!
//! [`FoundryLocalManager`] is a singleton that initialises the native core
//! library, provides access to the model [`Catalog`], and can start / stop
//! the local web service.

use std::sync::atomic::AtomicBool;
use std::sync::{Arc, Mutex};

use crate::catalog::Catalog;
use crate::configuration::{FoundryLocalConfig, Logger};
use crate::detail::api::Api;
use crate::detail::manager::{EpProgressCallback, NativeManager};
use crate::detail::task::spawn_blocking;
use crate::error::{FoundryLocalError, Result};
use crate::types::{EpDownloadResult, EpInfo};


/// Primary entry point for interacting with Foundry Local.
///
/// Created once via [`FoundryLocalManager::create`]; subsequent calls return
/// the existing instance.
pub struct FoundryLocalManager {
    native: Arc<NativeManager>,
    catalog: Catalog,
    urls: Mutex<Vec<String>>,
    /// Application logger (stub — not yet wired into the native core).
    _logger: Option<Box<dyn Logger>>,
}

type EpDownloadProgressCallback = Box<dyn FnMut(&str, f64) + Send + 'static>;

/// Builder for configuring and running execution provider downloads.
pub struct EpDownloadBuilder<'a> {
    manager: &'a FoundryLocalManager,
    names: Option<Vec<String>>,
    progress_callback: Option<EpDownloadProgressCallback>,
    cancel_flag: Option<Arc<AtomicBool>>,
}

impl<'a> EpDownloadBuilder<'a> {
    fn new(manager: &'a FoundryLocalManager) -> Self {
        Self {
            manager,
            names: None,
            progress_callback: None,
            cancel_flag: None,
        }
    }

    /// Download only the named execution providers.
    pub fn names<I, S>(mut self, names: I) -> Self
    where
        I: IntoIterator<Item = S>,
        S: Into<String>,
    {
        self.names = Some(names.into_iter().map(Into::into).collect());
        self
    }

    /// Report per-EP download progress as `(ep_name, percent)`.
    pub fn progress<F>(mut self, callback: F) -> Self
    where
        F: FnMut(&str, f64) + Send + 'static,
    {
        self.progress_callback = Some(Box::new(callback));
        self
    }

    /// Cancel the download when `cancel_flag` is set to `true`.
    pub fn cancel(mut self, cancel_flag: Arc<AtomicBool>) -> Self {
        self.cancel_flag = Some(cancel_flag);
        self
    }

    /// Run the configured execution provider download.
    pub async fn run(self) -> Result<EpDownloadResult> {
        self.manager
            .download_and_register_eps_impl(self.names, self.progress_callback, self.cancel_flag)
            .await
    }
}

impl FoundryLocalManager {
    /// Initialise the SDK.
    ///
    /// The first call creates the singleton, loads the native library, runs
    /// the initialisation, and builds the model catalog.  Subsequent calls
    /// return a reference to the same instance (the provided config is
    /// ignored after the first call).
    pub fn create(config: FoundryLocalConfig) -> Result<Self> {
        let mut config = config;
        let api = Arc::new(Api::load(config.library_path_ref())?);
        let logger = config.take_logger();
        let native_config = config.build_native(&api)?;

        let native = Arc::new(NativeManager::create(
            Arc::clone(&api),
            native_config.as_ptr(),
        )?);

        let catalog_ptr = native.catalog_ptr()?;
        let catalog = Catalog::new(Arc::clone(&api), catalog_ptr)?;

        // Owned manager: teardown runs via RAII when this value is dropped
        // (mid-program), not via a process-exit atexit hook. This releases the
        // native manager (and unregisters EPs) while the engine/Metal runtime is
        // still alive — matching the C++ SDK's local-`Manager` semantics, and
        // avoiding the WebGPU `ReleaseEpFactory` teardown throw (ORT #29206).
        Ok(FoundryLocalManager {
            native,
            catalog,
            urls: Mutex::new(Vec::new()),
            _logger: logger,
        })
    }

    /// Access the model catalog.
    pub fn catalog(&self) -> &Catalog {
        &self.catalog
    }

    /// Begin a graceful shutdown of the local engine.
    ///
    /// Stops the web service, prevents new model loads, stops existing
    /// sessions, and unloads models. Idempotent and safe to call from any
    /// thread.
    ///
    /// Calling this is optional: the native manager is released automatically
    /// at process exit. Use it when you want to deterministically wind the
    /// engine down before exiting. After calling `shutdown`, the manager should
    /// not be used for further inference.
    pub fn shutdown(&self) -> Result<()> {
        self.native.shutdown()
    }

    /// URLs that the local web service is listening on.
    ///
    /// Empty until [`Self::start_web_service`] has been called.
    pub fn urls(&self) -> Result<Vec<String>> {
        let lock = self.urls.lock().map_err(|_| FoundryLocalError::Internal {
            reason: "Failed to acquire urls lock".into(),
        })?;
        Ok(lock.clone())
    }

    /// Start the local web service.
    ///
    /// The listening URLs are stored internally and can be retrieved via
    /// [`Self::urls`] after this method returns.
    pub async fn start_web_service(&self) -> Result<()> {
        let native = Arc::clone(&self.native);
        let urls = spawn_blocking(move || {
            native.web_service_start()?;
            native.web_service_urls()
        })
        .await?;
        *self.urls.lock().map_err(|_| FoundryLocalError::Internal {
            reason: "Failed to acquire urls lock".into(),
        })? = urls;
        Ok(())
    }

    /// Stop the local web service.
    pub async fn stop_web_service(&self) -> Result<()> {
        let native = Arc::clone(&self.native);
        spawn_blocking(move || native.web_service_stop()).await?;
        self.urls
            .lock()
            .map_err(|_| FoundryLocalError::Internal {
                reason: "Failed to acquire urls lock".into(),
            })?
            .clear();
        Ok(())
    }

    /// Discover available execution providers and their registration status.
    pub fn discover_eps(&self) -> Result<Vec<EpInfo>> {
        self.native.discover_eps()
    }

    /// Download and register execution providers.
    ///
    /// If `names` is `None` or empty, all available EPs are downloaded.
    /// Otherwise only the named EPs are downloaded and registered.
    pub async fn download_and_register_eps(
        &self,
        names: Option<&[&str]>,
    ) -> Result<EpDownloadResult> {
        let names = names.map(|n| n.iter().map(|s| s.to_string()).collect::<Vec<_>>());
        self.download_and_register_eps_impl(names, None, None).await
    }

    /// Download and register execution providers, reporting per-EP progress.
    ///
    /// If `names` is `None` or empty, all available EPs are downloaded.
    /// Otherwise only the named EPs are downloaded and registered.
    ///
    /// `progress_callback` receives `(ep_name, percent)` where `percent`
    /// ranges from 0.0 to 100.0 as each EP downloads.
    pub async fn download_and_register_eps_with_progress<F>(
        &self,
        names: Option<&[&str]>,
        progress_callback: F,
    ) -> Result<EpDownloadResult>
    where
        F: FnMut(&str, f64) + Send + 'static,
    {
        let names = names.map(|n| n.iter().map(|s| s.to_string()).collect::<Vec<_>>());
        self.download_and_register_eps_impl(names, Some(Box::new(progress_callback)), None)
            .await
    }

    /// Configure and run execution provider downloads with a builder.
    ///
    /// Use this for call sites that need names, progress, cancellation, or
    /// future download options.
    pub fn download_and_register_eps_builder(&self) -> EpDownloadBuilder<'_> {
        EpDownloadBuilder::new(self)
    }

    async fn download_and_register_eps_impl(
        &self,
        names: Option<Vec<String>>,
        progress_callback: Option<EpDownloadProgressCallback>,
        cancel_flag: Option<Arc<AtomicBool>>,
    ) -> Result<EpDownloadResult> {
        let native = Arc::clone(&self.native);

        // Snapshot requested EP names (default: all discoverable).
        let requested: Vec<String> = match &names {
            Some(n) if !n.is_empty() => n.clone(),
            _ => native
                .discover_eps()
                .unwrap_or_default()
                .into_iter()
                .map(|e| e.name)
                .collect(),
        };

        let (message, after) = spawn_blocking(move || {
            let name_refs: Option<Vec<&str>> = names
                .as_ref()
                .map(|n| n.iter().map(String::as_str).collect());
            let progress: Option<EpProgressCallback> =
                progress_callback.map(|cb| cb as EpProgressCallback);
            let message =
                native.download_and_register_eps(name_refs.as_deref(), progress, cancel_flag);
            // Re-query registration state to synthesise the per-EP result.
            let after = native.discover_eps().unwrap_or_default();
            Ok::<(Option<String>, Vec<EpInfo>), FoundryLocalError>((message, after))
        })
        .await?;

        let registered_eps: Vec<String> = requested
            .iter()
            .filter(|name| after.iter().any(|e| &e.name == *name && e.is_registered))
            .cloned()
            .collect();
        let failed_eps: Vec<String> = requested
            .iter()
            .filter(|name| !registered_eps.contains(*name))
            .cloned()
            .collect();

        let success = message.is_none() && failed_eps.is_empty();
        let status = match &message {
            None => "All requested execution providers were registered successfully.".to_string(),
            Some(msg) if msg.is_empty() => {
                "One or more execution providers failed to register.".to_string()
            }
            Some(msg) => msg.clone(),
        };

        let result = EpDownloadResult {
            success,
            status,
            registered_eps,
            failed_eps,
        };

        // Invalidate the catalog cache if any EP was newly registered so the next
        // access re-fetches models with the updated set of available EPs.
        if result.success || !result.registered_eps.is_empty() {
            let _ = self.catalog.update_models().await;
        }

        Ok(result)
    }
}

