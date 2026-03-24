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
use crate::huggingface_catalog::HuggingFaceCatalog;

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
    model_load_manager: Arc<ModelLoadManager>,
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
            model_load_manager,
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

    /// Create a separate HuggingFace catalog for registering and downloading
    /// models from HuggingFace.
    ///
    /// The three-step flow:
    /// 1. `add_catalog("https://huggingface.co", None)` — create the catalog
    /// 2. `catalog.register_model("org/repo")` — register (config-only download)
    /// 3. `model.download(None)` — download ONNX files
    ///
    /// The returned catalog is owned by the caller. Each call creates a new
    /// instance with registrations loaded from disk.
    pub async fn add_catalog(
        &self,
        catalog_url: &str,
        token: Option<String>,
    ) -> Result<HuggingFaceCatalog> {
        if !catalog_url.to_lowercase().contains("huggingface.co") {
            return Err(FoundryLocalError::Validation {
                reason: format!(
                    "Unsupported catalog URL '{}'. Only HuggingFace catalogs (huggingface.co) are supported.",
                    catalog_url
                ),
            });
        }

        HuggingFaceCatalog::create(
            Arc::clone(&self.core),
            Arc::clone(&self.model_load_manager),
            token,
        )
        .await
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
}
