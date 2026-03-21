//! A single model variant backed by [`ModelInfo`].

use std::fmt;
use std::path::PathBuf;
use std::sync::Arc;

use serde_json::json;

use crate::catalog::CacheInvalidator;
use crate::detail::core_interop::CoreInterop;
use crate::detail::ModelLoadManager;
use crate::error::Result;
use crate::openai::AudioClient;
use crate::openai::ChatClient;
use crate::types::ModelInfo;

/// Represents one specific variant of a model (a particular id within an alias
/// group).
#[derive(Clone)]
pub struct ModelVariant {
    info: ModelInfo,
    core: Arc<CoreInterop>,
    model_load_manager: Arc<ModelLoadManager>,
    cache_invalidator: CacheInvalidator,
}

impl fmt::Debug for ModelVariant {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("ModelVariant")
            .field("id", &self.id())
            .field("alias", &self.alias())
            .finish()
    }
}

impl ModelVariant {
    pub(crate) fn new(
        info: ModelInfo,
        core: Arc<CoreInterop>,
        model_load_manager: Arc<ModelLoadManager>,
        cache_invalidator: CacheInvalidator,
    ) -> Self {
        Self {
            info,
            core,
            model_load_manager,
            cache_invalidator,
        }
    }

    /// The full [`ModelInfo`] metadata for this variant.
    pub fn info(&self) -> &ModelInfo {
        &self.info
    }

    /// Unique identifier.
    pub fn id(&self) -> &str {
        &self.info.id
    }

    /// Alias shared with sibling variants.
    pub fn alias(&self) -> &str {
        &self.info.alias
    }

    /// Check whether the variant is cached locally by querying the native
    /// core.
    ///
    /// Each call performs a full IPC round-trip. When checking many variants,
    /// prefer [`Catalog::get_cached_models`] which fetches the full list in a
    /// single call.
    pub async fn is_cached(&self) -> Result<bool> {
        let raw = self
            .core
            .execute_command_async("get_cached_models".into(), None)
            .await?;
        if raw.trim().is_empty() {
            return Ok(false);
        }
        let cached_ids: Vec<String> = serde_json::from_str(&raw)?;
        Ok(cached_ids.iter().any(|id| id == &self.info.id))
    }

    /// Check whether the variant is currently loaded into memory.
    pub async fn is_loaded(&self) -> Result<bool> {
        let loaded = self.model_load_manager.list_loaded().await?;
        Ok(loaded.iter().any(|id| id == &self.info.id))
    }

    /// Download the model variant. If `progress` is provided, it receives
    /// human-readable progress strings as the download proceeds.
    pub async fn download<F>(&self, progress: Option<F>) -> Result<()>
    where
        F: FnMut(&str) + Send + 'static,
    {
        let model_param = if self.info.provider_type.eq_ignore_ascii_case("huggingface") {
            &self.info.uri
        } else {
            &self.info.id
        };
        let params = json!({ "Params": { "Model": model_param } });
        match progress {
            Some(cb) => {
                self.core
                    .execute_command_streaming_async("download_model".into(), Some(params), cb)
                    .await?;
            }
            None => {
                self.core
                    .execute_command_async("download_model".into(), Some(params))
                    .await?;
            }
        }
        self.cache_invalidator.invalidate();
        Ok(())
    }

    /// Return the local file-system path where this variant is stored.
    pub async fn path(&self) -> Result<PathBuf> {
        let params = json!({ "Params": { "Model": self.info.id } });
        let path_str = self
            .core
            .execute_command_async("get_model_path".into(), Some(params))
            .await?;
        Ok(PathBuf::from(path_str))
    }

    /// Load the variant into memory.
    pub async fn load(&self) -> Result<()> {
        self.model_load_manager.load(&self.info.id).await
    }

    /// Unload the variant from memory.
    pub async fn unload(&self) -> Result<String> {
        self.model_load_manager.unload(&self.info.id).await
    }

    /// Remove the variant from the local cache.
    pub async fn remove_from_cache(&self) -> Result<String> {
        let params = json!({ "Params": { "Model": self.info.id } });
        let result = self
            .core
            .execute_command_async("remove_cached_model".into(), Some(params))
            .await?;
        self.cache_invalidator.invalidate();
        Ok(result)
    }

    /// Create a [`ChatClient`] bound to this variant.
    pub fn create_chat_client(&self) -> ChatClient {
        ChatClient::new(self.info.id.clone(), Arc::clone(&self.core))
    }

    /// Create an [`AudioClient`] bound to this variant.
    pub fn create_audio_client(&self) -> AudioClient {
        AudioClient::new(self.info.id.clone(), Arc::clone(&self.core))
    }
}
