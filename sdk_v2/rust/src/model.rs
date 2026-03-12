//! High-level model abstraction that wraps one or more [`ModelVariant`]s
//! sharing the same alias.

use std::path::PathBuf;
use std::sync::Arc;

use crate::detail::core_interop::CoreInterop;
use crate::error::{FoundryLocalError, Result};
use crate::model_variant::ModelVariant;
use crate::openai::AudioClient;
use crate::openai::ChatClient;

/// A model groups one or more [`ModelVariant`]s that share the same alias.
///
/// By default the variant that is already cached locally is selected.  You
/// can override the selection with [`Model::select_variant`].
#[derive(Debug, Clone)]
pub struct Model {
    alias: String,
    core: Arc<CoreInterop>,
    variants: Vec<ModelVariant>,
    selected_index: usize,
}

impl Model {
    pub(crate) fn new(
        alias: String,
        core: Arc<CoreInterop>,
    ) -> Self {
        Self {
            alias,
            core,
            variants: Vec::new(),
            selected_index: 0,
        }
    }

    /// Add a variant.  If the new variant is cached and the current selection
    /// is not, the new variant becomes the selected one.
    pub(crate) fn add_variant(&mut self, variant: ModelVariant) {
        self.variants.push(variant);
        let new_idx = self.variants.len() - 1;

        // Prefer a cached variant over a non-cached one.
        if self.variants[new_idx].info().cached && !self.variants[self.selected_index].info().cached
        {
            self.selected_index = new_idx;
        }
    }

    /// Select a variant by its unique id.
    pub fn select_variant(&mut self, id: &str) -> Result<()> {
        if let Some(pos) = self.variants.iter().position(|v| v.id() == id) {
            self.selected_index = pos;
            return Ok(());
        }
        let available: Vec<String> = self.variants.iter().map(|v| v.id().to_string()).collect();
        Err(FoundryLocalError::ModelOperation {
            reason: format!(
                "Variant '{id}' not found for model '{}'. Available: {available:?}",
                self.alias
            ),
        })
    }

    /// Returns a reference to the currently selected variant.
    pub fn selected_variant(&self) -> &ModelVariant {
        &self.variants[self.selected_index]
    }

    /// Returns all variants that belong to this model.
    pub fn variants(&self) -> &[ModelVariant] {
        &self.variants
    }

    /// Alias shared by all variants in this model.
    pub fn alias(&self) -> &str {
        &self.alias
    }

    /// Unique identifier of the selected variant.
    pub fn id(&self) -> &str {
        self.selected_variant().id()
    }

    /// Whether the selected variant is cached on disk.
    pub async fn is_cached(&self) -> Result<bool> {
        self.selected_variant().is_cached().await
    }

    /// Whether the selected variant is loaded into memory.
    pub async fn is_loaded(&self) -> Result<bool> {
        self.selected_variant().is_loaded().await
    }

    /// Download the selected variant. If `progress` is provided, it receives
    /// human-readable progress strings as they arrive from the native core.
    pub async fn download<F>(&self, progress: Option<F>) -> Result<()>
    where
        F: FnMut(&str) + Send + 'static,
    {
        self.selected_variant().download(progress).await
    }

    /// Return the local file-system path of the selected variant.
    pub async fn path(&self) -> Result<PathBuf> {
        self.selected_variant().path().await
    }

    /// Load the selected variant into memory.
    pub async fn load(&self) -> Result<()> {
        self.selected_variant().load().await
    }

    /// Unload the selected variant from memory.
    pub async fn unload(&self) -> Result<String> {
        self.selected_variant().unload().await
    }

    /// Remove the selected variant from the local cache.
    pub async fn remove_from_cache(&self) -> Result<String> {
        self.selected_variant().remove_from_cache().await
    }

    /// Create a [`ChatClient`] bound to the selected variant.
    pub fn create_chat_client(&self) -> ChatClient {
        ChatClient::new(self.id().to_string(), Arc::clone(&self.core))
    }

    /// Create an [`AudioClient`] bound to the selected variant.
    pub fn create_audio_client(&self) -> AudioClient {
        AudioClient::new(self.id().to_string(), Arc::clone(&self.core))
    }
}
