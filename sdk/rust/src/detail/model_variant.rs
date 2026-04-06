//! A single model variant backed by [`ModelInfo`].
//!
//! This type is an implementation detail.  Public APIs return
//! [`Arc<Model>`](crate::Model) instead.

use std::fmt;
use std::path::PathBuf;
use std::sync::atomic::AtomicBool;
use std::sync::Arc;

use serde_json::json;

use super::core_interop::CoreInterop;
use super::ModelLoadManager;
use crate::catalog::CacheInvalidator;
use crate::error::Result;
use crate::openai::AudioClient;
use crate::openai::ChatClient;
use crate::types::ModelInfo;

/// Represents one specific variant of a model (a particular id within an alias
/// group).
///
/// This is an implementation detail — callers should use [`Model`](crate::Model).
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
            .field("id", &self.info.id)
            .field("alias", &self.info.alias)
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

    pub(crate) fn id(&self) -> &str {
        &self.info.id
    }

    pub(crate) fn alias(&self) -> &str {
        &self.info.alias
    }

    pub(crate) fn info(&self) -> &ModelInfo {
        &self.info
    }

    pub(crate) fn info_ref(&self) -> &ModelInfo {
        &self.info
    }

    pub(crate) async fn is_cached(&self) -> Result<bool> {
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

    pub(crate) async fn is_loaded(&self) -> Result<bool> {
        let loaded = self.model_load_manager.list_loaded().await?;
        Ok(loaded.iter().any(|id| id == &self.info.id))
    }

    pub(crate) async fn download<F>(&self, progress: Option<F>) -> Result<()>
    where
        F: FnMut(&str) + Send + 'static,
    {
        self.download_impl(progress, None).await
    }

    /// Like [`Self::download`], but accepts a shared cancellation flag.
    /// When `cancel_flag` is set to `true`, the download will be cancelled at
    /// the next progress callback.
    pub(crate) async fn download_cancellable<F>(
        &self,
        progress: Option<F>,
        cancel_flag: Arc<AtomicBool>,
    ) -> Result<()>
    where
        F: FnMut(&str) + Send + 'static,
    {
        self.download_impl(progress, Some(cancel_flag)).await
    }

    async fn download_impl<F>(
        &self,
        progress: Option<F>,
        cancel_flag: Option<Arc<AtomicBool>>,
    ) -> Result<()>
    where
        F: FnMut(&str) + Send + 'static,
    {
        let params = json!({ "Params": { "Model": self.info.id } });
        match (progress, cancel_flag) {
            (Some(cb), Some(flag)) => {
                self.core
                    .execute_command_streaming_cancellable_async(
                        "download_model".into(),
                        Some(params),
                        cb,
                        flag,
                    )
                    .await?;
            }
            (Some(cb), None) => {
                self.core
                    .execute_command_streaming_async("download_model".into(), Some(params), cb)
                    .await?;
            }
            (None, Some(flag)) => {
                // Use a no-op callback to engage the callback mechanism
                // required for cancellation checks.
                self.core
                    .execute_command_streaming_cancellable_async(
                        "download_model".into(),
                        Some(params),
                        |_: &str| {},
                        flag,
                    )
                    .await?;
            }
            (None, None) => {
                self.core
                    .execute_command_async("download_model".into(), Some(params))
                    .await?;
            }
        }
        self.cache_invalidator.invalidate();
        Ok(())
    }

    pub(crate) async fn path(&self) -> Result<PathBuf> {
        let params = json!({ "Params": { "Model": self.info.id } });
        let path_str = self
            .core
            .execute_command_async("get_model_path".into(), Some(params))
            .await?;
        Ok(PathBuf::from(path_str))
    }

    pub(crate) async fn load(&self) -> Result<()> {
        self.model_load_manager.load(&self.info.id).await
    }

    pub(crate) async fn unload(&self) -> Result<String> {
        self.model_load_manager.unload(&self.info.id).await
    }

    pub(crate) async fn remove_from_cache(&self) -> Result<String> {
        let params = json!({ "Params": { "Model": self.info.id } });
        let result = self
            .core
            .execute_command_async("remove_cached_model".into(), Some(params))
            .await?;
        self.cache_invalidator.invalidate();
        Ok(result)
    }

    pub(crate) fn create_chat_client(&self) -> ChatClient {
        ChatClient::new(&self.info.id, Arc::clone(&self.core))
    }

    pub(crate) fn create_audio_client(&self) -> AudioClient {
        AudioClient::new(&self.info.id, Arc::clone(&self.core))
    }
}
