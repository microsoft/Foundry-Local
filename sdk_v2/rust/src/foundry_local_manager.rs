//! Top-level entry point for the Foundry Local SDK.
//!
//! [`FoundryLocalManager`] is a singleton that initialises the native core
//! library, provides access to the model [`Catalog`], and can start / stop
//! the local web service.

use std::sync::{Arc, Mutex, OnceLock, Once};

use serde_json::json;

use crate::catalog::Catalog;
use crate::configuration::{Configuration, FoundryLocalConfig};
use crate::detail::core_interop::CoreInterop;
use crate::detail::ModelLoadManager;
use crate::error::{FoundryLocalError, Result};

/// Global singleton holder.
static INSTANCE: OnceLock<std::result::Result<FoundryLocalManager, String>> = OnceLock::new();
static INIT_ONCE: Once = Once::new();

/// Primary entry point for interacting with Foundry Local.
///
/// Created once via [`FoundryLocalManager::create`]; subsequent calls return
/// the existing instance.
pub struct FoundryLocalManager {
    core: Arc<CoreInterop>,
    catalog: Catalog,
    urls: Mutex<Vec<String>>,
}

impl FoundryLocalManager {
    /// Initialise the SDK.
    ///
    /// The first call creates the singleton, loads the native library, runs
    /// the `initialize` command, and builds the model catalog.  Subsequent
    /// calls return a reference to the same instance (the provided config is
    /// ignored after the first call).
    pub fn create(config: FoundryLocalConfig) -> Result<&'static Self> {
        // Use `Once` + `OnceLock` to ensure initialisation runs at most once,
        // eliminating the TOCTOU race between `get()` and `set()`.
        INIT_ONCE.call_once(|| {
            let result = (|| -> Result<FoundryLocalManager> {
                let mut internal_config = Configuration::new(config)?;
                let core = Arc::new(CoreInterop::new(&mut internal_config)?);

                // Send the configuration map to the native core.
                let init_params = json!({ "Params": internal_config.params });
                core.execute_command("initialize", Some(&init_params))?;

                let service_endpoint = internal_config.params.get("WebServiceExternalUrl").cloned();

                let model_load_manager =
                    Arc::new(ModelLoadManager::new(Arc::clone(&core), service_endpoint));

                let catalog = Catalog::new(Arc::clone(&core), Arc::clone(&model_load_manager))?;

                Ok(FoundryLocalManager {
                    core,
                    catalog,
                    urls: Mutex::new(Vec::new()),
                })
            })();

            let _ = INSTANCE.set(result.map_err(|e| e.to_string()));
        });

        match INSTANCE.get() {
            Some(Ok(manager)) => Ok(manager),
            Some(Err(msg)) => Err(FoundryLocalError::CommandExecution {
                reason: format!("SDK initialization failed: {msg}"),
            }),
            None => Err(FoundryLocalError::CommandExecution {
                reason: "SDK initialization not completed".into(),
            }),
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

    /// Start the local web service and return the listening URLs.
    pub async fn start_web_service(&self) -> Result<Vec<String>> {
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
        })? = parsed.clone();
        Ok(parsed)
    }

    /// Stop the local web service.
    pub async fn stop_web_service(&self) -> Result<()> {
        self.core
            .execute_command_async("stop_service".into(), None)
            .await?;
        self.urls.lock().map_err(|_| FoundryLocalError::Internal {
            reason: "Failed to acquire urls lock".into(),
        })?.clear();
        Ok(())
    }
}
