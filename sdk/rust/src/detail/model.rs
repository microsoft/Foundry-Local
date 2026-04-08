//! Public model type backed by an internal enum.
//!
//! Users interact solely with [`Model`].  The internal representation
//! distinguishes between a single variant and a group of variants sharing
//! the same alias, but callers never need to know which kind they hold.

use std::fmt;
use std::path::PathBuf;
use std::sync::atomic::{AtomicUsize, Ordering::Relaxed};
use std::sync::Arc;

use super::core_interop::CoreInterop;
use super::model_variant::ModelVariant;
use crate::error::{FoundryLocalError, Result};
use crate::openai::AudioClient;
use crate::openai::ChatClient;
use crate::openai::EmbeddingClient;
use crate::openai::LiveAudioTranscriptionSession;
use crate::types::ModelInfo;

/// The public model type.
///
/// A `Model` may represent either a group of variants (as returned by
/// [`Catalog::get_model`](crate::Catalog::get_model)) or a single variant (as
/// returned by [`Catalog::get_model_variant`](crate::Catalog::get_model_variant)
/// or [`Model::variants`]).
///
/// When a `Model` groups multiple variants, operations are forwarded to
/// the currently selected variant.  Use [`variants`](Model::variants) to
/// inspect the available variants and [`select_variant`](Model::select_variant)
/// to change the selection.
pub struct Model {
    inner: ModelKind,
}

#[allow(clippy::large_enum_variant)]
enum ModelKind {
    /// A single model variant (from `get_model_variant` or `variants()`).
    ModelVariant(ModelVariant),
    /// A group of variants sharing the same alias (from `get_model`).
    Model {
        alias: String,
        core: Arc<CoreInterop>,
        variants: Vec<ModelVariant>,
        selected: AtomicUsize,
    },
}

impl Clone for Model {
    fn clone(&self) -> Self {
        Self {
            inner: match &self.inner {
                ModelKind::ModelVariant(v) => ModelKind::ModelVariant(v.clone()),
                ModelKind::Model {
                    alias,
                    core,
                    variants,
                    selected,
                } => ModelKind::Model {
                    alias: alias.clone(),
                    core: Arc::clone(core),
                    variants: variants.clone(),
                    selected: AtomicUsize::new(selected.load(Relaxed)),
                },
            },
        }
    }
}

impl fmt::Debug for Model {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match &self.inner {
            ModelKind::ModelVariant(v) => f
                .debug_struct("Model::ModelVariant")
                .field("id", &v.id())
                .field("alias", &v.alias())
                .finish(),
            ModelKind::Model {
                alias,
                variants,
                selected,
                ..
            } => f
                .debug_struct("Model::Model")
                .field("alias", alias)
                .field("id", &variants[selected.load(Relaxed)].id())
                .field("variants_count", &variants.len())
                .field("selected_index", &selected.load(Relaxed))
                .finish(),
        }
    }
}

// ── Construction (crate-internal) ────────────────────────────────────────────

impl Model {
    /// Create a `Model` wrapping a single variant.
    pub(crate) fn from_variant(variant: ModelVariant) -> Self {
        Self {
            inner: ModelKind::ModelVariant(variant),
        }
    }

    /// Create a `Model` grouping multiple variants under one alias.
    pub(crate) fn from_group(alias: String, core: Arc<CoreInterop>) -> Self {
        Self {
            inner: ModelKind::Model {
                alias,
                core,
                variants: Vec::new(),
                selected: AtomicUsize::new(0),
            },
        }
    }

    /// Add a variant to a group.  Panics if called on a `ModelVariant` kind.
    ///
    /// If the new variant is cached and the current selection is not, the new
    /// variant becomes the selected one.
    pub(crate) fn add_variant(&mut self, variant: ModelVariant) {
        match &mut self.inner {
            ModelKind::Model {
                variants, selected, ..
            } => {
                variants.push(variant);
                let new_idx = variants.len() - 1;
                let current = selected.load(Relaxed);
                if variants[new_idx].info_ref().cached && !variants[current].info_ref().cached {
                    selected.store(new_idx, Relaxed);
                }
            }
            ModelKind::ModelVariant(_) => {
                panic!("add_variant called on a single-variant Model");
            }
        }
    }
}

// ── Private helpers ──────────────────────────────────────────────────────────

impl Model {
    fn selected_variant(&self) -> &ModelVariant {
        match &self.inner {
            ModelKind::ModelVariant(v) => v,
            ModelKind::Model {
                variants, selected, ..
            } => &variants[selected.load(Relaxed)],
        }
    }
}

// ── Public API ───────────────────────────────────────────────────────────────

impl Model {
    /// Unique identifier of the (selected) variant.
    pub fn id(&self) -> &str {
        self.selected_variant().id()
    }

    /// Alias shared by all variants of this model.
    pub fn alias(&self) -> &str {
        match &self.inner {
            ModelKind::ModelVariant(v) => v.alias(),
            ModelKind::Model { alias, .. } => alias,
        }
    }

    /// Full catalog metadata for the (selected) variant.
    pub fn info(&self) -> &ModelInfo {
        self.selected_variant().info()
    }

    /// Maximum context length (in tokens), or `None` if unknown.
    pub fn context_length(&self) -> Option<u64> {
        self.selected_variant().info().context_length
    }

    /// Comma-separated input modalities (e.g. `"text,image"`), or `None`.
    pub fn input_modalities(&self) -> Option<&str> {
        self.selected_variant().info().input_modalities.as_deref()
    }

    /// Comma-separated output modalities (e.g. `"text"`), or `None`.
    pub fn output_modalities(&self) -> Option<&str> {
        self.selected_variant().info().output_modalities.as_deref()
    }

    /// Capability tags (e.g. `"reasoning"`), or `None`.
    pub fn capabilities(&self) -> Option<&str> {
        self.selected_variant().info().capabilities.as_deref()
    }

    /// Whether the model supports tool/function calling, or `None`.
    pub fn supports_tool_calling(&self) -> Option<bool> {
        self.selected_variant().info().supports_tool_calling
    }

    /// Whether the (selected) variant is cached on disk.
    pub async fn is_cached(&self) -> Result<bool> {
        self.selected_variant().is_cached().await
    }

    /// Whether the (selected) variant is loaded into memory.
    pub async fn is_loaded(&self) -> Result<bool> {
        self.selected_variant().is_loaded().await
    }

    /// Download the (selected) variant.  If `progress` is provided it
    /// receives download progress as a percentage (0.0–100.0).
    pub async fn download<F>(&self, progress: Option<F>) -> Result<()>
    where
        F: FnMut(f64) + Send + 'static,
    {
        self.selected_variant().download(progress).await
    }

    /// Return the local file-system path of the (selected) variant.
    pub async fn path(&self) -> Result<PathBuf> {
        self.selected_variant().path().await
    }

    /// Load the (selected) variant into memory.
    pub async fn load(&self) -> Result<()> {
        self.selected_variant().load().await
    }

    /// Unload the (selected) variant from memory.
    pub async fn unload(&self) -> Result<String> {
        self.selected_variant().unload().await
    }

    /// Remove the (selected) variant from the local cache.
    pub async fn remove_from_cache(&self) -> Result<String> {
        self.selected_variant().remove_from_cache().await
    }

    /// Create a [`ChatClient`] bound to the (selected) variant.
    pub fn create_chat_client(&self) -> ChatClient {
        self.selected_variant().create_chat_client()
    }

    /// Create an [`AudioClient`] bound to the (selected) variant.
    pub fn create_audio_client(&self) -> AudioClient {
        self.selected_variant().create_audio_client()
    }

    /// Create an [`EmbeddingClient`] bound to the (selected) variant.
    pub fn create_embedding_client(&self) -> EmbeddingClient {
        self.selected_variant().create_embedding_client()
    }

    /// Create a [`LiveAudioTranscriptionSession`] bound to the (selected) variant.
    ///
    /// Configure the session's [`settings`](LiveAudioTranscriptionSession::settings)
    /// before calling [`start`](LiveAudioTranscriptionSession::start).
    pub fn create_live_transcription_session(&self) -> LiveAudioTranscriptionSession {
        self.selected_variant().create_live_transcription_session()
    }
    }

    /// Available variants of this model.
    ///
    /// For a single-variant model (e.g. from
    /// [`Catalog::get_model_variant`](crate::Catalog::get_model_variant)),
    /// this returns a single-element list containing itself.
    pub fn variants(&self) -> Vec<Arc<Model>> {
        match &self.inner {
            ModelKind::ModelVariant(v) => {
                vec![Arc::new(Model::from_variant(v.clone()))]
            }
            ModelKind::Model { variants, .. } => variants
                .iter()
                .map(|v| Arc::new(Model::from_variant(v.clone())))
                .collect(),
        }
    }

    /// Select a variant to use for subsequent operations.
    ///
    /// The `variant` must be one of the models returned by [`variants`](Model::variants).
    ///
    /// # Errors
    ///
    /// Returns an error if the variant does not belong to this model.
    /// For single-variant models this always returns an error — use
    /// [`Catalog::get_model`](crate::Catalog::get_model) to obtain a model
    /// with all variants available.
    pub fn select_variant(&self, variant: &Model) -> Result<()> {
        self.select_variant_by_id(variant.id())
    }

    /// Select a variant by its unique id string.
    ///
    /// This is a convenience method for cases where you have a variant id
    /// from an external source. Prefer [`select_variant`](Model::select_variant)
    /// when you already have a [`Model`] reference from [`variants`](Model::variants).
    ///
    /// # Errors
    ///
    /// Returns an error if no variant with the given id exists.
    /// For single-variant models this always returns an error — use
    /// [`Catalog::get_model`](crate::Catalog::get_model) to obtain a model
    /// with all variants available.
    pub fn select_variant_by_id(&self, id: &str) -> Result<()> {
        match &self.inner {
            ModelKind::ModelVariant(v) => Err(FoundryLocalError::ModelOperation {
                reason: format!(
                    "select_variant is not supported on a single variant. \
                     Call Catalog::get_model(\"{}\") to get a model with all variants available.",
                    v.alias()
                ),
            }),
            ModelKind::Model {
                variants,
                selected,
                alias,
                ..
            } => match variants.iter().position(|v| v.id() == id) {
                Some(pos) => {
                    selected.store(pos, Relaxed);
                    Ok(())
                }
                None => {
                    let available: Vec<&str> = variants.iter().map(|v| v.id()).collect();
                    Err(FoundryLocalError::ModelOperation {
                            reason: format!(
                                "Variant '{id}' not found for model '{alias}'. Available: {available:?}",
                            ),
                        })
                }
            },
        }
    }
}
