//! A single model variant backed by [`ModelInfo`].
//!
//! This type is an implementation detail.  Public APIs return
//! [`Arc<dyn IModel>`](crate::IModel) instead.

use std::fmt;
use std::future::Future;
use std::path::PathBuf;
use std::pin::Pin;
use std::sync::Arc;

use serde_json::json;

use crate::catalog::CacheInvalidator;
use super::core_interop::CoreInterop;
use super::ModelLoadManager;
use crate::error::{FoundryLocalError, Result};
use crate::imodel::IModel;
use crate::openai::AudioClient;
use crate::openai::ChatClient;
use crate::types::ModelInfo;

/// Represents one specific variant of a model (a particular id within an alias
/// group).
///
/// This is an implementation detail — callers should use the [`IModel`] trait.
#[derive(Clone)]
pub(crate) struct ModelVariant {
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

    pub(crate) fn info_ref(&self) -> &ModelInfo {
        &self.info
    }

    /// Download with a generic progress callback (used internally).
    pub(crate) async fn download_generic<F>(&self, progress: Option<F>) -> Result<()>
    where
        F: FnMut(&str) + Send + 'static,
    {
        let params = json!({ "Params": { "Model": self.info.id } });
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
}

#[allow(clippy::manual_async_fn)]
impl IModel for ModelVariant {
    fn id(&self) -> &str {
        &self.info.id
    }

    fn alias(&self) -> &str {
        &self.info.alias
    }

    fn info(&self) -> &ModelInfo {
        &self.info
    }

    fn context_length(&self) -> Option<u64> {
        self.info.context_length
    }

    fn input_modalities(&self) -> Option<&str> {
        self.info.input_modalities.as_deref()
    }

    fn output_modalities(&self) -> Option<&str> {
        self.info.output_modalities.as_deref()
    }

    fn capabilities(&self) -> Option<&str> {
        self.info.capabilities.as_deref()
    }

    fn supports_tool_calling(&self) -> Option<bool> {
        self.info.supports_tool_calling
    }

    fn is_cached(&self) -> Pin<Box<dyn Future<Output = Result<bool>> + Send + '_>> {
        Box::pin(async move {
            let raw = self
                .core
                .execute_command_async("get_cached_models".into(), None)
                .await?;
            if raw.trim().is_empty() {
                return Ok(false);
            }
            let cached_ids: Vec<String> = serde_json::from_str(&raw)?;
            Ok(cached_ids.iter().any(|id| id == &self.info.id))
        })
    }

    fn is_loaded(&self) -> Pin<Box<dyn Future<Output = Result<bool>> + Send + '_>> {
        Box::pin(async move {
            let loaded = self.model_load_manager.list_loaded().await?;
            Ok(loaded.iter().any(|id| id == &self.info.id))
        })
    }

    fn download(
        &self,
        progress: Option<Box<dyn FnMut(&str) + Send>>,
    ) -> Pin<Box<dyn Future<Output = Result<()>> + Send + '_>> {
        Box::pin(async move {
            let params = json!({ "Params": { "Model": self.info.id } });
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
        })
    }

    fn path(&self) -> Pin<Box<dyn Future<Output = Result<PathBuf>> + Send + '_>> {
        Box::pin(async move {
            let params = json!({ "Params": { "Model": self.info.id } });
            let path_str = self
                .core
                .execute_command_async("get_model_path".into(), Some(params))
                .await?;
            Ok(PathBuf::from(path_str))
        })
    }

    fn load(&self) -> Pin<Box<dyn Future<Output = Result<()>> + Send + '_>> {
        Box::pin(async move { self.model_load_manager.load(&self.info.id).await })
    }

    fn unload(&self) -> Pin<Box<dyn Future<Output = Result<String>> + Send + '_>> {
        Box::pin(async move { self.model_load_manager.unload(&self.info.id).await })
    }

    fn remove_from_cache(&self) -> Pin<Box<dyn Future<Output = Result<String>> + Send + '_>> {
        Box::pin(async move {
            let params = json!({ "Params": { "Model": self.info.id } });
            let result = self
                .core
                .execute_command_async("remove_cached_model".into(), Some(params))
                .await?;
            self.cache_invalidator.invalidate();
            Ok(result)
        })
    }

    fn create_chat_client(&self) -> ChatClient {
        ChatClient::new(&self.info.id, Arc::clone(&self.core))
    }

    fn create_audio_client(&self) -> AudioClient {
        AudioClient::new(&self.info.id, Arc::clone(&self.core))
    }

    fn variants(&self) -> Vec<Arc<dyn IModel>> {
        vec![Arc::new(self.clone())]
    }

    fn select_variant(&self, _id: &str) -> Result<()> {
        Err(FoundryLocalError::ModelOperation {
            reason: format!(
                "select_variant is not supported on a single variant. \
                 Call Catalog::get_model(\"{}\") to get a Model with all variants available.",
                self.alias()
            ),
        })
    }
}
