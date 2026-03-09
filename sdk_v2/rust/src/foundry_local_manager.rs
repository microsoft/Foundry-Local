//! Top-level entry point for the Foundry Local SDK.
//!
//! [`FoundryLocalManager`] is a singleton that initialises the native core
//! library, provides access to the model [`Catalog`], and can start / stop
//! the local web service.

use std::sync::{Arc, OnceLock};

use serde_json::json;

use crate::catalog::Catalog;
use crate::configuration::{Configuration, FoundryLocalConfig};
use crate::detail::core_interop::CoreInterop;
use crate::detail::ModelLoadManager;
use crate::error::Result;

/// Global singleton holder.
static INSTANCE: OnceLock<FoundryLocalManager> = OnceLock::new();

/// Primary entry point for interacting with Foundry Local.
///
/// Created once via [`FoundryLocalManager::create`]; subsequent calls return
/// the existing instance.
pub struct FoundryLocalManager {
    _config: Configuration,
    core: Arc<CoreInterop>,
    _model_load_manager: Arc<ModelLoadManager>,
    catalog: Catalog,
    urls: std::sync::Mutex<Vec<String>>,
}

impl FoundryLocalManager {
    /// Initialise the SDK.
    ///
    /// The first call creates the singleton, loads the native library, runs
    /// the `initialize` command, and builds the model catalog.  Subsequent
    /// calls return a reference to the same instance (the provided config is
    /// ignored after the first call).
    pub fn create(config: FoundryLocalConfig) -> Result<&'static Self> {
        // If already initialised, return the existing instance.
        if let Some(mgr) = INSTANCE.get() {
            return Ok(mgr);
        }

        let internal_config = Configuration::new(config)?;
        let core = Arc::new(CoreInterop::new(&internal_config)?);

        // Send the configuration map to the native core.
        let init_params = json!({ "Params": internal_config.params });
        core.execute_command("initialize", Some(&init_params))?;

        let service_endpoint = internal_config
            .params
            .get("WebServiceExternalUrl")
            .cloned();

        let model_load_manager = Arc::new(ModelLoadManager::new(
            Arc::clone(&core),
            service_endpoint,
        ));

        let catalog = Catalog::new(Arc::clone(&core), Arc::clone(&model_load_manager))?;

        let manager = Self {
            _config: internal_config,
            core,
            _model_load_manager: model_load_manager,
            catalog,
            urls: std::sync::Mutex::new(Vec::new()),
        };

        // Attempt to store; if another thread raced us, return whichever won.
        match INSTANCE.set(manager) {
            Ok(()) => Ok(INSTANCE.get().unwrap()),
            Err(_) => Ok(INSTANCE.get().unwrap()),
        }
    }

    /// Access the model catalog.
    pub fn catalog(&self) -> &Catalog {
        &self.catalog
    }

    /// URLs that the local web service is listening on.
    ///
    /// Empty until [`Self::start_web_service`] has been called.
    pub fn urls(&self) -> Vec<String> {
        self.urls.lock().unwrap().clone()
    }

    /// Start the local web service and return the listening URLs.
    pub async fn start_web_service(&self) -> Result<Vec<String>> {
        let raw = self
            .core
            .execute_command_async("start_service".into(), None)
            .await?;
        let parsed: Vec<String> = if raw.trim().is_empty() {
            Vec::new()
        } else {
            serde_json::from_str(&raw).unwrap_or_else(|_| vec![raw])
        };
        *self.urls.lock().unwrap() = parsed.clone();
        Ok(parsed)
    }

    /// Stop the local web service.
    pub async fn stop_web_service(&self) -> Result<()> {
        self.core
            .execute_command_async("stop_service".into(), None)
            .await?;
        self.urls.lock().unwrap().clear();
        Ok(())
    }
}
