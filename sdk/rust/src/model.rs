//! High-level model abstraction that wraps one or more [`ModelVariant`]s
//! sharing the same alias.

use std::fmt;
use std::path::PathBuf;
use std::sync::atomic::{AtomicUsize, Ordering};
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
            selected_index: AtomicUsize::new(self.selected_index.load(Ordering::Relaxed)),
        }
    }
}

impl fmt::Debug for Model {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Model")
            .field("alias", &self.alias())
            .field("id", &self.id())
            .field("variants_count", &self.variants.len())
            .field("selected_index", &self.selected_index.load(Ordering::Relaxed))
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
        let current = self.selected_index.load(Ordering::Relaxed);

        // Prefer a cached variant over a non-cached one.
        if self.variants[new_idx].info().cached && !self.variants[current].info().cached {
            self.selected_index.store(new_idx, Ordering::Relaxed);
        }
    }

    /// Select a variant by its unique id.
    pub fn select_variant(&self, id: &str) -> Result<()> {
        match self.variants.iter().position(|v| v.id() == id) {
            Some(pos) => {
                self.selected_index.store(pos, Ordering::Relaxed);
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

    /// Returns a reference to the currently selected variant.
    pub fn selected_variant(&self) -> &ModelVariant {
        &self.variants[self.selected_index.load(Ordering::Relaxed)]
    }

    /// Returns all variants that belong to this model.
    pub fn variants(&self) -> &[Arc<ModelVariant>] {
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
        ChatClient::new(self.id(), Arc::clone(&self.core))
    }

    /// Create an [`AudioClient`] bound to the selected variant.
    pub fn create_audio_client(&self) -> AudioClient {
        AudioClient::new(self.id(), Arc::clone(&self.core))
    }
}
