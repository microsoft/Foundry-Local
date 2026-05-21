//! Top-level entry point for the Foundry Local SDK.
//!
//! [`FoundryLocalManager`] is a singleton that initialises the native core
//! library, provides access to the model [`Catalog`], and can start / stop
//! the local web service.

use std::sync::atomic::AtomicBool;
use std::sync::{Arc, Mutex, OnceLock};

use serde_json::json;

use crate::catalog::Catalog;
use crate::configuration::{Configuration, FoundryLocalConfig, Logger};
use crate::detail::core_interop::CoreInterop;
use crate::detail::ModelLoadManager;
use crate::error::{FoundryLocalError, Result};
use crate::types::{EpDownloadResult, EpInfo};

/// Global singleton holder — only stores a successfully initialised manager.
static INSTANCE: OnceLock<FoundryLocalManager> = OnceLock::new();
/// Guard to ensure only one thread attempts initialisation at a time.
static INIT_GUARD: Mutex<()> = Mutex::new(());

/// Primary entry point for interacting with Foundry Local.
///
/// Created once via [`FoundryLocalManager::create`]; subsequent calls return
/// the existing instance.
pub struct FoundryLocalManager {
    core: Arc<CoreInterop>,
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
        let names: Option<Vec<&str>> = self
            .names
            .as_ref()
            .map(|names| names.iter().map(String::as_str).collect());
        self.manager
            .download_and_register_eps_impl(
                names.as_deref(),
                self.progress_callback,
                self.cancel_flag,
            )
            .await
    }
}

impl FoundryLocalManager {
    /// Initialise the SDK.
    ///
    /// The first call creates the singleton, loads the native library, runs
    /// the `initialize` command, and builds the model catalog.  Subsequent
    /// calls return a reference to the same instance (the provided config is
    /// ignored after the first call).
    pub fn create(config: FoundryLocalConfig) -> Result<&'static Self> {
        // Fast path: singleton already initialised.
        if let Some(manager) = INSTANCE.get() {
            return Ok(manager);
        }

        // Slow path: acquire init guard so only one thread attempts initialisation.
        let _guard = INIT_GUARD.lock().map_err(|_| FoundryLocalError::Internal {
            reason: "initialisation guard poisoned".into(),
        })?;

        // Double-check after acquiring the lock.
        if let Some(manager) = INSTANCE.get() {
            return Ok(manager);
        }

        let (mut internal_config, logger) = Configuration::new(config)?;
        let core = Arc::new(CoreInterop::new(&mut internal_config)?);

        // Send the configuration map to the native core.
        let init_params = json!({ "Params": internal_config.params });
        core.execute_command("initialize", Some(&init_params))?;

        let service_endpoint = internal_config.params.get("WebServiceExternalUrl").cloned();

        let model_load_manager =
            Arc::new(ModelLoadManager::new(Arc::clone(&core), service_endpoint));

        let catalog = Catalog::new(Arc::clone(&core), Arc::clone(&model_load_manager))?;

        let manager = FoundryLocalManager {
            core,
            catalog,
            urls: Mutex::new(Vec::new()),
            _logger: logger,
        };

        // Only cache on success — failures allow the next caller to retry.
        match INSTANCE.set(manager) {
            Ok(()) => Ok(INSTANCE.get().unwrap()),
            Err(_) => {
                // Another thread beat us — return their instance.
                Ok(INSTANCE.get().unwrap())
            }
        }
    }

    /// Access the model catalog.
    pub fn catalog(&self) -> &Catalog {
        &self.catalog
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
        let raw = self
            .core
            .execute_command_async("start_service".into(), None)
            .await?;
        let parsed: Vec<String> = if raw.trim().is_empty() {
            Vec::new()
        } else {
            serde_json::from_str(&raw)?
        };
        *self.urls.lock().map_err(|_| FoundryLocalError::Internal {
            reason: "Failed to acquire urls lock".into(),
        })? = parsed;
        Ok(())
    }

    /// Stop the local web service.
    pub async fn stop_web_service(&self) -> Result<()> {
        self.core
            .execute_command_async("stop_service".into(), None)
            .await?;
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
        let raw = self.core.execute_command("discover_eps", None)?;
        let eps: Vec<EpInfo> = serde_json::from_str(&raw)?;
        Ok(eps)
    }

    /// Download and register execution providers.
    ///
    /// If `names` is `None` or empty, all available EPs are downloaded.
    /// Otherwise only the named EPs are downloaded and registered.
    pub async fn download_and_register_eps(
        &self,
        names: Option<&[&str]>,
    ) -> Result<EpDownloadResult> {
        self.download_and_register_eps_impl(names, None::<fn(&str, f64)>, None)
            .await
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
        self.download_and_register_eps_impl(names, Some(progress_callback), None)
            .await
    }

    /// Configure and run execution provider downloads with a builder.
    ///
    /// Use this for call sites that need names, progress, cancellation, or
    /// future download options.
    pub fn download_and_register_eps_builder(&self) -> EpDownloadBuilder<'_> {
        EpDownloadBuilder::new(self)
    }

    async fn download_and_register_eps_impl<F>(
        &self,
        names: Option<&[&str]>,
        progress_callback: Option<F>,
        cancel_flag: Option<Arc<AtomicBool>>,
    ) -> Result<EpDownloadResult>
    where
        F: FnMut(&str, f64) + Send + 'static,
    {
        let params = match names {
            Some(n) if !n.is_empty() => Some(json!({ "Params": { "Names": n.join(",") } })),
            _ => None,
        };

        let raw = match (progress_callback, cancel_flag) {
            (Some(cb), Some(flag)) => {
                let mut callback = cb;
                let wrapper = move |chunk: &str| {
                    if let Some(sep) = chunk.find('|') {
                        let name = &chunk[..sep];
                        if let Ok(percent) = chunk[sep + 1..].parse::<f64>() {
                            callback(if name.is_empty() { "" } else { name }, percent);
                        }
                    }
                };

                self.core
                    .execute_command_streaming_cancellable_async(
                        "download_and_register_eps".into(),
                        params,
                        wrapper,
                        flag,
                    )
                    .await?
            }
            (Some(cb), None) => {
                let mut callback = cb;
                let wrapper = move |chunk: &str| {
                    if let Some(sep) = chunk.find('|') {
                        let name = &chunk[..sep];
                        if let Ok(percent) = chunk[sep + 1..].parse::<f64>() {
                            callback(if name.is_empty() { "" } else { name }, percent);
                        }
                    }
                };

                self.core
                    .execute_command_streaming_async(
                        "download_and_register_eps".into(),
                        params,
                        wrapper,
                    )
                    .await?
            }
            (None, Some(flag)) => {
                self.core
                    .execute_command_streaming_cancellable_async(
                        "download_and_register_eps".into(),
                        params,
                        |_chunk: &str| {},
                        flag,
                    )
                    .await?
            }
            (None, None) => {
                self.core
                    .execute_command_async("download_and_register_eps".into(), params)
                    .await?
            }
        };

        let result: EpDownloadResult = serde_json::from_str(&raw)?;

        // Invalidate the catalog cache if any EP was newly registered so the next
        // access re-fetches models with the updated set of available EPs.
        if result.success || !result.registered_eps.is_empty() {
            self.catalog.invalidate_cache();
        }

        Ok(result)
    }
}
