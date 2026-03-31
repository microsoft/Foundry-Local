//! Top-level entry point for the Foundry Local SDK.
//!
//! [`FoundryLocalManager`] is a singleton that initialises the native core
//! library, provides access to the model [`Catalog`], and can start / stop
//! the local web service.

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

    /// Download and register execution providers. This is a blocking call.
    ///
    /// If `names` is `None` or empty, all available EPs are downloaded.
    /// Otherwise only the named EPs are downloaded and registered.
    pub fn download_and_register_eps(&self, names: Option<&[&str]>) -> Result<EpDownloadResult> {
        let params = match names {
            Some(n) if !n.is_empty() => Some(json!({ "Params": { "Names": n.join(",") } })),
            _ => None,
        };
        let raw = self
            .core
            .execute_command("download_and_register_eps", params.as_ref())?;
        let result: EpDownloadResult = serde_json::from_str(&raw)?;

        // Invalidate the catalog cache if any EP was newly registered so the next
        // access re-fetches models with the updated set of available EPs.
        if result.success || !result.registered_eps.is_empty() {
            self.catalog.invalidate_cache();
        }

        Ok(result)
    }

    /// Download and register execution providers with per-EP progress reporting.
    ///
    /// The `progress_callback` is invoked with `(ep_name, percent)` where
    /// `percent` ranges from 0.0 to 100.0. If `names` is `None` or empty,
    /// all available EPs are downloaded.
    pub async fn download_and_register_eps_with_progress<F>(
        &self,
        names: Option<&[&str]>,
        progress_callback: F,
    ) -> Result<()>
    where
        F: FnMut(&str, f64) + Send + 'static,
    {
        let params = match names {
            Some(n) if !n.is_empty() => {
                Some(json!({ "Params": { "Names": n.join(",") } }))
            }
            _ => None,
        };

        let callback = {
            let mut cb = progress_callback;
            move |chunk: &str| {
                if let Some(sep) = chunk.find('|') {
                    let name = &chunk[..sep];
                    if let Ok(percent) = chunk[sep + 1..].parse::<f64>() {
                        cb(if name.is_empty() { "" } else { name }, percent);
                    }
                }
            }
        };

        self.core
            .execute_command_streaming_async(
                "download_and_register_eps".into(),
                params,
                callback,
            )
            .await?;

        Ok(())
    }
}
