//! Manages model loading and unloading.
//!
//! When an external service URL is configured the manager delegates to HTTP
//! endpoints (`models/load/{id}`, `models/unload/{id}`, `models/loaded`).
//! Otherwise it falls through to the native core library via [`CoreInterop`].

use std::sync::Arc;

use serde_json::json;

use crate::detail::core_interop::CoreInterop;
use crate::error::Result;

/// Manages the lifecycle of loaded models.
#[derive(Debug)]
pub struct ModelLoadManager {
    core: Arc<CoreInterop>,
    external_service_url: Option<String>,
    client: reqwest::Client,
}

impl ModelLoadManager {
    pub(crate) fn new(core: Arc<CoreInterop>, external_service_url: Option<String>) -> Self {
        Self {
            core,
            external_service_url,
            client: reqwest::Client::new(),
        }
    }

    /// Load a model by its identifier.
    pub async fn load(&self, model_id: &str) -> Result<()> {
        if let Some(base_url) = &self.external_service_url {
            let encoded_id = urlencoding::encode(model_id);
            self.http_get(&format!("{base_url}/models/load/{encoded_id}"))
                .await?;
            return Ok(());
        }
        let params = json!({ "Params": { "Model": model_id } });
        self.core
            .execute_command_async("load_model".into(), Some(params))
            .await?;
        Ok(())
    }

    /// Unload a previously loaded model.
    pub async fn unload(&self, model_id: &str) -> Result<String> {
        if let Some(base_url) = &self.external_service_url {
            let encoded_id = urlencoding::encode(model_id);
            return self
                .http_get(&format!("{base_url}/models/unload/{encoded_id}"))
                .await;
        }
        let params = json!({ "Params": { "Model": model_id } });
        self.core
            .execute_command_async("unload_model".into(), Some(params))
            .await
    }

    /// Return the list of currently loaded model identifiers.
    pub async fn list_loaded(&self) -> Result<Vec<String>> {
        let raw = if let Some(base_url) = &self.external_service_url {
            self.http_get(&format!("{base_url}/models/loaded")).await?
        } else {
            self.core
                .execute_command_async("list_loaded_models".into(), None)
                .await?
        };

        if raw.trim().is_empty() {
            return Ok(Vec::new());
        }

        let ids: Vec<String> = serde_json::from_str(&raw)?;
        Ok(ids)
    }

    async fn http_get(&self, url: &str) -> Result<String> {
        let body = self
            .client
            .get(url)
            .send()
            .await?
            .error_for_status()?
            .text()
            .await?;
        Ok(body)
    }
}
