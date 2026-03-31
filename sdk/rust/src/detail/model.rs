//! High-level model abstraction that wraps one or more model variants
//! sharing the same alias.

use std::fmt;
use std::future::Future;
use std::path::PathBuf;
use std::pin::Pin;
use std::sync::atomic::{AtomicUsize, Ordering::Relaxed};
use std::sync::Arc;

use super::core_interop::CoreInterop;
use crate::error::{FoundryLocalError, Result};
use crate::imodel::IModel;
use super::model_variant::ModelVariant;
use crate::openai::AudioClient;
use crate::openai::ChatClient;
use crate::types::ModelInfo;

/// A model groups one or more variants that share the same alias.
///
/// By default the variant that is already cached locally is selected.  You
/// can override the selection with [`IModel::select_variant`].
///
/// Implements [`IModel`] — all operations are forwarded to the currently
/// selected variant.
pub struct Model {
    alias: String,
    core: Arc<CoreInterop>,
    variants: Vec<Arc<ModelVariant>>,
    selected_index: AtomicUsize,
}

impl Clone for Model {
    fn clone(&self) -> Self {
        Self {
            alias: self.alias.clone(),
            core: Arc::clone(&self.core),
            variants: self.variants.clone(),
            selected_index: AtomicUsize::new(self.selected_index.load(Relaxed)),
        }
    }
}

impl fmt::Debug for Model {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Model")
            .field("alias", &self.alias())
            .field("id", &self.id())
            .field("variants_count", &self.variants.len())
            .field("selected_index", &self.selected_index.load(Relaxed))
            .finish()
    }
}

impl Model {
    pub(crate) fn new(alias: String, core: Arc<CoreInterop>) -> Self {
        Self {
            alias,
            core,
            variants: Vec::new(),
            selected_index: AtomicUsize::new(0),
        }
    }

    /// Add a variant.  If the new variant is cached and the current selection
    /// is not, the new variant becomes the selected one.
    pub(crate) fn add_variant(&mut self, variant: Arc<ModelVariant>) {
        self.variants.push(variant);
        let new_idx = self.variants.len() - 1;
        let current = self.selected_index.load(Relaxed);

        // Prefer a cached variant over a non-cached one.
        if self.variants[new_idx].info_ref().cached && !self.variants[current].info_ref().cached {
            self.selected_index.store(new_idx, Relaxed);
        }
    }

    /// Returns a reference to the currently selected variant (crate-internal).
    pub(crate) fn selected_variant(&self) -> &ModelVariant {
        &self.variants[self.selected_index.load(Relaxed)]
    }

    /// Download the selected variant with a generic progress callback.
    ///
    /// This is a convenience method that avoids the boxing overhead of the
    /// trait method when the concrete type is known.
    pub async fn download_generic<F>(&self, progress: Option<F>) -> Result<()>
    where
        F: FnMut(&str) + Send + 'static,
    {
        self.selected_variant().download_generic(progress).await
    }
}

#[allow(clippy::manual_async_fn)]
impl IModel for Model {
    fn id(&self) -> &str {
        self.selected_variant().id()
    }

    fn alias(&self) -> &str {
        &self.alias
    }

    fn info(&self) -> &ModelInfo {
        self.selected_variant().info()
    }

    fn context_length(&self) -> Option<u64> {
        self.selected_variant().info().context_length
    }

    fn input_modalities(&self) -> Option<&str> {
        self.selected_variant().info().input_modalities.as_deref()
    }

    fn output_modalities(&self) -> Option<&str> {
        self.selected_variant().info().output_modalities.as_deref()
    }

    fn capabilities(&self) -> Option<&str> {
        self.selected_variant().info().capabilities.as_deref()
    }

    fn supports_tool_calling(&self) -> Option<bool> {
        self.selected_variant().info().supports_tool_calling
    }

    fn is_cached(&self) -> Pin<Box<dyn Future<Output = Result<bool>> + Send + '_>> {
        self.selected_variant().is_cached()
    }

    fn is_loaded(&self) -> Pin<Box<dyn Future<Output = Result<bool>> + Send + '_>> {
        self.selected_variant().is_loaded()
    }

    fn download(
        &self,
        progress: Option<Box<dyn FnMut(&str) + Send>>,
    ) -> Pin<Box<dyn Future<Output = Result<()>> + Send + '_>> {
        self.selected_variant().download(progress)
    }

    fn path(&self) -> Pin<Box<dyn Future<Output = Result<PathBuf>> + Send + '_>> {
        self.selected_variant().path()
    }

    fn load(&self) -> Pin<Box<dyn Future<Output = Result<()>> + Send + '_>> {
        self.selected_variant().load()
    }

    fn unload(&self) -> Pin<Box<dyn Future<Output = Result<String>> + Send + '_>> {
        self.selected_variant().unload()
    }

    fn remove_from_cache(&self) -> Pin<Box<dyn Future<Output = Result<String>> + Send + '_>> {
        self.selected_variant().remove_from_cache()
    }

    fn create_chat_client(&self) -> ChatClient {
        ChatClient::new(self.id(), Arc::clone(&self.core))
    }

    fn create_audio_client(&self) -> AudioClient {
        AudioClient::new(self.id(), Arc::clone(&self.core))
    }

    fn variants(&self) -> Vec<Arc<dyn IModel>> {
        self.variants
            .iter()
            .map(|v| Arc::clone(v) as Arc<dyn IModel>)
            .collect()
    }

    /// Select a variant by its unique id.
    fn select_variant(&self, id: &str) -> Result<()> {
        match self.variants.iter().position(|v| v.id() == id) {
            Some(pos) => {
                self.selected_index.store(pos, Relaxed);
                Ok(())
            }
            None => {
                let available: Vec<&str> = self.variants.iter().map(|v| v.id()).collect();
                Err(FoundryLocalError::ModelOperation {
                    reason: format!(
                        "Variant '{id}' not found for model '{}'. Available: {available:?}",
                        self.alias
                    ),
                })
            }
        }
    }
}
