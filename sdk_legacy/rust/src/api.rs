use std::{collections::HashMap, env};

use anyhow::{anyhow, Result};
use log::{debug, info};
use serde_json::Value;

use crate::{
    client::HttpClient,
    models::{ExecutionProvider, FoundryListResponseModel, FoundryModelInfo},
    service::{check_foundry_installed, get_service_uri, start_service},
};

/// Manager for Foundry Local SDK operations.
pub struct FoundryLocalManager {
    service_uri: Option<String>,
    client: Option<HttpClient>,
    catalog_list: Option<Vec<FoundryModelInfo>>,
    catalog_dict: Option<HashMap<String, FoundryModelInfo>>,
    timeout: Option<u64>,
}

/// Builder for creating a FoundryLocalManager instance.
pub struct FoundryLocalManagerBuilder {
    alias_or_model_id: Option<String>,
    bootstrap: bool,
    timeout_secs: Option<u64>,
}

impl FoundryLocalManagerBuilder {
    /// Create a new builder instance.
    pub fn new() -> Self {
        Self {
            alias_or_model_id: None,
            bootstrap: false,
            timeout_secs: None,
        }
    }

    /// Set the alias or model ID to download and load.
    pub fn alias_or_model_id(mut self, alias_or_model_id: impl Into<String>) -> Self {
        self.alias_or_model_id = Some(alias_or_model_id.into());
        self
    }

    /// Set whether to start the service if it is not running.
    pub fn bootstrap(mut self, bootstrap: bool) -> Self {
        self.bootstrap = bootstrap;
        self
    }

    /// Set the timeout for the HTTP client in seconds.
    pub fn timeout_secs(mut self, timeout_secs: u64) -> Self {
        self.timeout_secs = Some(timeout_secs);
        self
    }

    /// Build the FoundryLocalManager instance.
    pub async fn build(self) -> Result<FoundryLocalManager> {
        check_foundry_installed()?;

        let mut manager = FoundryLocalManager {
            service_uri: None,
            client: None,
            catalog_list: None,
            catalog_dict: None,
            timeout: self.timeout_secs,
        };

        if let Some(uri) = get_service_uri() {
            manager.set_service_uri_and_client(Some(uri));
        }

        if self.bootstrap {
            manager.start_service()?;

            if let Some(model) = self.alias_or_model_id {
                manager.download_model(&model, None, false).await?;
                manager.load_model(&model, Some(600)).await?;
            }
        }

        Ok(manager)
    }
}

/// Extract numeric version from ID (e.g. model-x:3 → 3)
///
/// # Returns
///
/// Numeric version as i32, or -1 if not found.
fn get_version(model_id: &str) -> i32 {
    match model_id.rsplit(':').next() {
        Some(version_str) => version_str.parse::<i32>().unwrap_or(-1),
        None => -1,
    }
}

impl FoundryLocalManager {
    /// Create a new builder for FoundryLocalManager.
    pub fn builder() -> FoundryLocalManagerBuilder {
        FoundryLocalManagerBuilder::new()
    }

    fn set_service_uri_and_client(&mut self, service_uri: Option<String>) {
        self.service_uri = service_uri.clone();
        self.client = service_uri.map(|uri| HttpClient::new(&uri, self.timeout));
    }

    /// Get the service URI.
    ///
    /// # Returns
    ///
    /// URI of the Foundry service.
    pub fn service_uri(&self) -> Result<&str> {
        self.service_uri
            .as_deref()
            .ok_or_else(|| anyhow!("Service URI is not set. Please start the service first."))
    }

    /// Get the HTTP client.
    ///
    /// # Returns
    ///
    /// HTTP client instance.
    fn client(&self) -> Result<&HttpClient> {
        self.client
            .as_ref()
            .ok_or_else(|| anyhow!("HTTP client is not set. Please start the service first."))
    }

    /// Get the endpoint for the service.
    ///
    /// # Returns
    ///
    /// Endpoint URL.
    pub fn endpoint(&self) -> Result<String> {
        Ok(format!("{}/v1", self.service_uri()?))
    }

    /// Get the API key for authentication.
    ///
    /// # Returns
    ///
    /// API key.
    pub fn api_key(&self) -> String {
        env::var("OPENAI_API_KEY").unwrap_or_else(|_| "OPENAI_API_KEY".to_string())
    }

    // Service management API

    /// Check if the service is running. Will also set the service URI if it is not set.
    ///
    /// # Returns
    ///
    /// True if the service is running, False otherwise.
    pub fn is_service_running(&mut self) -> bool {
        if let Some(uri) = get_service_uri() {
            self.set_service_uri_and_client(Some(uri));
            true
        } else {
            false
        }
    }

    /// Start the service.
    ///
    /// # Returns
    ///
    /// Result indicating success or failure.
    pub fn start_service(&mut self) -> Result<()> {
        let uri = start_service()?;
        self.set_service_uri_and_client(Some(uri));
        Ok(())
    }

    // Catalog API

    /// Get a list of available models in the catalog.
    ///
    /// # Returns
    ///
    /// List of catalog models.
    pub async fn list_catalog_models(&mut self) -> Result<&Vec<FoundryModelInfo>> {
        if self.catalog_list.is_none() {
            let models: Vec<FoundryListResponseModel> = self
                .client()?
                .get("/foundry/list", None)
                .await?
                .ok_or_else(|| anyhow!("Failed to get catalog models"))?;

            self.catalog_list = Some(
                models
                    .iter()
                    .map(FoundryModelInfo::from_list_response)
                    .collect(),
            );
        }

        Ok(self.catalog_list.as_ref().unwrap())
    }

    /// Get a dictionary of available models. Keyed by model ID and alias.
    /// Alias points to the most preferred model.
    ///
    /// # Returns
    ///
    /// Dictionary of catalog models.
    async fn get_catalog_dict(&mut self) -> Result<&HashMap<String, FoundryModelInfo>> {
        if let Some(ref dict) = self.catalog_dict {
            return Ok(dict);
        }

        let catalog_models = self.list_catalog_models().await?;
        let mut catalog_dict = HashMap::new();
        let mut alias_candidates: HashMap<String, Vec<FoundryModelInfo>> = HashMap::new();

        for model in catalog_models.iter() {
            catalog_dict.insert(model.id.clone(), model.clone());
            alias_candidates
                .entry(model.alias.clone())
                .or_default()
                .push(model.clone());
        }

        // Set up priority order
        let mut preferred_order = vec![
            ExecutionProvider::QNN,
            ExecutionProvider::CUDA,
            ExecutionProvider::CPU,
            ExecutionProvider::WebGPU,
        ];

        if cfg!(not(target_os = "windows")) {
            // Move CPU to the end for non-Windows
            preferred_order.retain(|p| *p != ExecutionProvider::CPU);
            preferred_order.push(ExecutionProvider::CPU);
        }

        let priority_map: HashMap<ExecutionProvider, usize> = preferred_order
            .into_iter()
            .enumerate()
            .map(|(i, provider)| (provider, i))
            .collect();

        // Choose the best candidate per alias
        for (alias, candidates) in alias_candidates {
            if let Some(best) = candidates.into_iter().max_by(|a, b| {
                // Compare priority (lower index = higher priority)
                let pa = priority_map.get(&a.runtime).copied().unwrap_or(usize::MAX);
                let pb = priority_map.get(&b.runtime).copied().unwrap_or(usize::MAX);
                match pa.cmp(&pb) {
                    std::cmp::Ordering::Less => std::cmp::Ordering::Greater,
                    std::cmp::Ordering::Greater => std::cmp::Ordering::Less,
                    std::cmp::Ordering::Equal => {
                        // Same priority → compare version
                        get_version(&a.id).cmp(&get_version(&b.id))
                    }
                }
            }) {
                catalog_dict.insert(alias, best);
            }
        }

        self.catalog_dict = Some(catalog_dict);
        Ok(self.catalog_dict.as_ref().unwrap())
    }

    /// Refresh the catalog.
    pub fn refresh_catalog(&mut self) {
        self.catalog_list = None;
        self.catalog_dict = None;
    }

    /// Get the model information by alias or ID.
    ///
    /// # Arguments
    ///
    /// * `alias_or_model_id` - Alias or Model ID. If it is an alias, the most preferred model will be returned.
    /// * `raise_on_not_found` - If true, raise an error if the model is not found. Default is false.
    ///
    /// # Returns
    ///
    /// Model information, or None if not found and raise_on_not_found is false.
    pub async fn get_model_info(
        &mut self,
        alias_or_model_id: &str,
        raise_on_not_found: bool,
    ) -> Result<FoundryModelInfo> {
        let catalog_dict = self.get_catalog_dict().await?;

        // Try to find exact match (alias or id)
        if let Some(model_info) = catalog_dict.get(alias_or_model_id) {
            return Ok(model_info.clone());
        }

        // If alias_or_model_id does NOT contain version suffix (":")
        if !alias_or_model_id.contains(':') {
            // Search for model IDs starting with alias_or_model_id + ":"
            let prefix = format!("{}:", alias_or_model_id);
            let mut best_version = -1;
            let mut best_model: Option<FoundryModelInfo> = None;

            for (key, info) in catalog_dict.iter() {
                if key.starts_with(&prefix) {
                    let version = get_version(key);
                    if version > best_version {
                        best_version = version;
                        best_model = Some(info.clone());
                    }
                }
            }

            if let Some(model) = best_model {
                return Ok(model);
            }
        }

        if raise_on_not_found {
            Err(anyhow!(
                "Model {} not found in the catalog",
                alias_or_model_id
            ))
        } else {
            Err(anyhow!(
                "Model {} not found in the catalog",
                alias_or_model_id
            ))
        }
    }

    /// Get the latest model information by alias or model ID.
    /// The difference from get_model_info is that this method will return the latest version of the model
    /// even when you pass it a model id that contains a version suffix.
    ///
    /// # Arguments
    ///
    /// * `alias_or_model_id` - Alias or Model ID. If it is an alias, the most preferred model will be returned.
    /// * `raise_on_not_found` - If true, raise an error if the model is not found. Default is false.
    ///
    /// # Returns
    ///
    /// Latest model information, or None if not found and raise_on_not_found is false.
    pub async fn get_latest_model_info(
        &mut self,
        alias_or_model_id: &str,
        raise_on_not_found: bool,
    ) -> Result<FoundryModelInfo> {
        if alias_or_model_id.is_empty() {
            if raise_on_not_found {
                return Err(anyhow!("The provided model alias or ID was empty."));
            } else {
                return Err(anyhow!(
                    "Model alias or ID was empty and raise_on_not_found is false."
                ));
            }
        }

        let catalog = self.get_catalog_dict().await?;

        if !alias_or_model_id.contains(':') {
            // Alias or ID without version suffix
            if let Some(model) = catalog.get(alias_or_model_id) {
                // Found directly in catalog (alias or id)
                Ok(model.clone())
            } else {
                // Fallback: try get_model_info which can look for latest version of ID without version
                self.get_model_info(alias_or_model_id, raise_on_not_found)
                    .await
            }
        } else {
            // ID with version suffix: strip version and get latest by ID without version
            let id_without_version = alias_or_model_id.split(':').next().unwrap_or("");
            self.get_model_info(id_without_version, raise_on_not_found)
                .await
        }
    }

    // Cache management API

    /// Get the cache location.
    ///
    /// # Returns
    ///
    /// Cache location as a string.
    pub async fn get_cache_location(&self) -> Result<String> {
        let response: Value = self
            .client()?
            .get("/foundry/cache", None)
            .await?
            .ok_or_else(|| anyhow!("Failed to get cache location"))?;

        response["location"]
            .as_str()
            .map(|s| s.to_string())
            .ok_or_else(|| anyhow!("Invalid cache location response"))
    }

    /// List cached models.
    ///
    /// # Returns
    ///
    /// List of cached models.
    pub async fn list_cached_models(&mut self) -> Result<Vec<FoundryModelInfo>> {
        let response: Value = self
            .client()?
            .get("/openai/models", None)
            .await?
            .ok_or_else(|| anyhow!("Failed to list cached models - no response"))?;

        // Handle both direct array response and object with models field
        let model_ids = if response.is_array() {
            response
                .as_array()
                .ok_or_else(|| anyhow!("Invalid models response - expected array"))?
                .iter()
                .filter_map(|v| v.as_str())
                .map(|s| s.to_string())
                .collect::<Vec<_>>()
        } else {
            response["models"]
                .as_array()
                .ok_or_else(|| anyhow!("Invalid models response - expected models field"))?
                .iter()
                .filter_map(|v| v.as_str())
                .map(|s| s.to_string())
                .collect::<Vec<_>>()
        };

        self.fetch_model_infos(&model_ids).await
    }

    async fn fetch_model_infos(&mut self, model_ids: &[String]) -> Result<Vec<FoundryModelInfo>> {
        let mut results = Vec::new();
        let catalog_dict = self.get_catalog_dict().await?;

        for id in model_ids {
            if let Some(model) = catalog_dict.get(id) {
                results.push(model.clone());
            } else {
                debug!("Model {id} not found in the catalog");
            }
        }

        Ok(results)
    }

    /// Download a model.
    ///
    /// # Arguments
    ///
    /// * `alias_or_model_id` - Alias or Model ID.
    /// * `token` - Optional token for authentication.
    /// * `force` - If true, force re-download even if the model is already cached.
    ///
    /// # Returns
    ///
    /// Downloaded model information.
    pub async fn download_model(
        &mut self,
        alias_or_model_id: &str,
        token: Option<&str>,
        force: bool,
    ) -> Result<FoundryModelInfo> {
        let model_info = self.get_model_info(alias_or_model_id, true).await?;
        info!(
            "Downloading model: {} ({}) - {} MB",
            model_info.alias, model_info.id, model_info.file_size_mb
        );

        let mut body = model_info.to_download_body();

        if let Some(t) = token {
            body["token"] = Value::String(t.to_string());
        }

        if force {
            body["Force"] = Value::Bool(true);
        }

        let client = self.client()?;
        let _response: Value = client
            .post_with_progress("/openai/download", Some(body))
            .await?;

        Ok(model_info)
    }

    /// Check if a newer version of a model is available.
    ///
    /// # Arguments
    ///
    /// * `alias_or_model_id` - Alias or Model ID.
    ///
    /// # Returns
    ///
    /// bool: True if a newer version is available, False otherwise.
    pub async fn is_model_upgradeable(&mut self, alias_or_model_id: &str) -> Result<bool> {
        info!("Checking if model '{}' is upgradeable", alias_or_model_id);

        // Get the latest model info (throws if not found)
        let latest_model_info = self.get_latest_model_info(alias_or_model_id, true).await?;

        let latest_version = get_version(&latest_model_info.id);
        if latest_version == -1 {
            return Ok(false); // Invalid version format
        }

        let cached_models = self.list_cached_models().await?;
        for cached_model in cached_models {
            if cached_model.id == latest_model_info.id
                && get_version(&cached_model.id) == latest_version
            {
                return Ok(false); // Already cached latest version
            }
        }

        Ok(true) // Latest version not in cache
    }

    /// Downloads the latest version of a model to the local cache, if it is not already cached.
    ///
    /// # Arguments
    ///
    /// * `alias_or_model_id` - The alias or model ID to upgrade.
    /// * `token` - Optional authentication token.
    ///
    /// # Errors
    ///
    /// Downloaded model information.
    pub async fn upgrade_model(
        &mut self,
        alias_or_model_id: &str,
        token: Option<&str>,
    ) -> Result<FoundryModelInfo> {
        // Get the latest model info (this also validates existence)
        let model_info = self.get_latest_model_info(alias_or_model_id, true).await?;

        // Download the model and discard the result
        self.download_model(&model_info.id, token, false).await
    }

    /// Load a model.
    ///
    /// # Arguments
    ///
    /// * `alias_or_model_id` - Alias or Model ID.
    /// * `ttl` - Optional time-to-live in seconds. Default is 10 minutes (600 seconds).
    ///
    /// # Returns
    ///
    /// Loaded model information.
    pub async fn load_model(
        &mut self,
        alias_or_model_id: &str,
        ttl: Option<i32>,
    ) -> Result<FoundryModelInfo> {
        let model_info = self.get_model_info(alias_or_model_id, true).await?;
        info!("Loading model: {} ({})", model_info.alias, model_info.id);

        let url = format!("/openai/load/{}", model_info.id);
        let ttl_str = ttl.unwrap_or(600).to_string();
        let mut query_params = vec![("ttl", ttl_str.as_str())];

        // Handle execution provider selection for WEBGPU and CUDA models
        let ep_str = if matches!(
            model_info.runtime,
            ExecutionProvider::WebGPU | ExecutionProvider::CUDA
        ) {
            let has_cuda_support = self
                .list_catalog_models()
                .await?
                .iter()
                .any(|mi| mi.runtime == ExecutionProvider::CUDA);

            if has_cuda_support {
                ExecutionProvider::CUDA.get_alias().to_string()
            } else {
                model_info.runtime.get_alias().to_string()
            }
        } else {
            String::new()
        };

        if !ep_str.is_empty() {
            query_params.push(("ep", ep_str.as_str()));
        }

        let client = self.client()?;
        let _response: Option<Value> = client.get(&url, Some(&query_params)).await?;

        Ok(model_info)
    }

    /// Unload a model.
    ///
    /// # Arguments
    ///
    /// * `alias_or_model_id` - Alias or Model ID.
    /// * `force` - If true, force unload even if the model is in use.
    ///
    /// # Returns
    ///
    /// Result indicating success or failure.
    pub async fn unload_model(&mut self, alias_or_model_id: &str, force: bool) -> Result<()> {
        let model_info = self.get_model_info(alias_or_model_id, true).await?;
        info!("Unloading model: {} ({})", model_info.alias, model_info.id);

        let url = format!("/openai/unload/{}", model_info.id);
        let force_str = force.to_string();
        let query_params = vec![("force", force_str.as_str())];

        let client = self.client()?;
        let _response: Option<Value> = client.get(&url, Some(&query_params)).await?;

        Ok(())
    }

    /// List loaded models.
    ///
    /// # Returns
    ///
    /// List of loaded models.
    pub async fn list_loaded_models(&mut self) -> Result<Vec<FoundryModelInfo>> {
        let response: Value = self
            .client()?
            .get("/openai/loadedmodels", None)
            .await?
            .ok_or_else(|| anyhow!("Failed to list loaded models - no response"))?;

        // Handle both direct array response and object with models field
        let model_ids = if response.is_array() {
            response
                .as_array()
                .ok_or_else(|| anyhow!("Invalid models response - expected array"))?
                .iter()
                .filter_map(|v| v.as_str())
                .map(|s| s.to_string())
                .collect::<Vec<_>>()
        } else {
            response["models"]
                .as_array()
                .ok_or_else(|| anyhow!("Invalid models response - expected models field"))?
                .iter()
                .filter_map(|v| v.as_str())
                .map(|s| s.to_string())
                .collect::<Vec<_>>()
        };

        self.fetch_model_infos(&model_ids).await
    }

    /// Set a custom service URI and client for testing purposes.
    #[doc(hidden)]
    pub fn set_test_service_uri(&mut self, uri: &str) {
        self.service_uri = Some(uri.to_string());
        self.client = Some(HttpClient::new(uri, self.timeout));
        self.catalog_list = None;
        self.catalog_dict = None;
    }

    /// Create a new FoundryLocalManager instance for testing without checking if Foundry is installed.
    #[doc(hidden)]
    pub async fn new_for_testing() -> Result<Self> {
        Ok(Self {
            service_uri: None,
            client: None,
            catalog_list: None,
            catalog_dict: None,
            timeout: None,
        })
    }
}
