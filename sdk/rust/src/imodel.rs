//! Public trait defining the model interface.
//!
//! All public APIs return [`Arc<dyn IModel>`] so that callers never need to
//! reference the internal `ModelVariant` type directly.

use std::fmt::Debug;
use std::future::Future;
use std::path::PathBuf;
use std::pin::Pin;
use std::sync::Arc;

use crate::error::Result;
use crate::openai::AudioClient;
use crate::openai::ChatClient;
use crate::types::ModelInfo;

/// Common interface for interacting with a model.
///
/// An `IModel` may represent either a group of variants (as returned by
/// [`Catalog::get_model`](crate::Catalog::get_model)) or a single variant (as
/// returned by [`Catalog::get_model_variant`](crate::Catalog::get_model_variant)
/// or [`IModel::variants`]).
///
/// When an `IModel` groups multiple variants, operations are forwarded to
/// the currently selected variant.  Use [`variants`](IModel::variants) to
/// inspect the available variants and [`select_variant`](IModel::select_variant)
/// to change the selection.
pub trait IModel: Send + Sync + Debug {
    /// Unique identifier of the (selected) variant.
    fn id(&self) -> &str;

    /// Alias shared by all variants of this model.
    fn alias(&self) -> &str;

    /// Full catalog metadata for the (selected) variant.
    fn info(&self) -> &ModelInfo;

    /// Maximum context length (in tokens), or `None` if unknown.
    fn context_length(&self) -> Option<u64>;

    /// Comma-separated input modalities (e.g. `"text,image"`), or `None`.
    fn input_modalities(&self) -> Option<&str>;

    /// Comma-separated output modalities (e.g. `"text"`), or `None`.
    fn output_modalities(&self) -> Option<&str>;

    /// Capability tags (e.g. `"reasoning"`), or `None`.
    fn capabilities(&self) -> Option<&str>;

    /// Whether the model supports tool/function calling, or `None`.
    fn supports_tool_calling(&self) -> Option<bool>;

    /// Whether the (selected) variant is cached on disk.
    fn is_cached(&self) -> Pin<Box<dyn Future<Output = Result<bool>> + Send + '_>>;

    /// Whether the (selected) variant is loaded into memory.
    fn is_loaded(&self) -> Pin<Box<dyn Future<Output = Result<bool>> + Send + '_>>;

    /// Download the (selected) variant.  If `progress` is provided it
    /// receives human-readable progress strings as they arrive.
    fn download(
        &self,
        progress: Option<Box<dyn FnMut(&str) + Send>>,
    ) -> Pin<Box<dyn Future<Output = Result<()>> + Send + '_>>;

    /// Return the local file-system path of the (selected) variant.
    fn path(&self) -> Pin<Box<dyn Future<Output = Result<PathBuf>> + Send + '_>>;

    /// Load the (selected) variant into memory.
    fn load(&self) -> Pin<Box<dyn Future<Output = Result<()>> + Send + '_>>;

    /// Unload the (selected) variant from memory.
    fn unload(&self) -> Pin<Box<dyn Future<Output = Result<String>> + Send + '_>>;

    /// Remove the (selected) variant from the local cache.
    fn remove_from_cache(&self) -> Pin<Box<dyn Future<Output = Result<String>> + Send + '_>>;

    /// Create a [`ChatClient`] bound to the (selected) variant.
    fn create_chat_client(&self) -> ChatClient;

    /// Create an [`AudioClient`] bound to the (selected) variant.
    fn create_audio_client(&self) -> AudioClient;

    /// Available variants of this model.
    ///
    /// For a single-variant model (e.g. from
    /// [`Catalog::get_model_variant`](crate::Catalog::get_model_variant)),
    /// this returns a single-element list containing itself.
    fn variants(&self) -> Vec<Arc<dyn IModel>>;

    /// Select a variant by its unique id.
    ///
    /// # Errors
    ///
    /// Returns an error if no variant with the given id exists in this model.
    /// For single-variant models this always returns an error — use
    /// [`Catalog::get_model`](crate::Catalog::get_model) to obtain a model
    /// with all variants available.
    fn select_variant(&self, id: &str) -> Result<()>;
}
