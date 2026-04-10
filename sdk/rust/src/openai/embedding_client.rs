//! OpenAI-compatible embedding client.

use std::sync::Arc;

use serde_json::{json, Value};

use crate::detail::core_interop::CoreInterop;
use crate::error::Result;

/// OpenAI-compatible embedding response.
#[derive(Debug, Clone, serde::Deserialize, serde::Serialize)]
pub struct EmbeddingData {
    /// The index of the embedding in the list.
    pub index: i32,
    /// The embedding vector.
    pub embedding: Vec<f64>,
}

/// OpenAI-compatible embedding response.
#[derive(Debug, Clone, serde::Deserialize, serde::Serialize)]
pub struct EmbeddingResponse {
    /// The model used for generation.
    pub model: String,
    /// The object type (always "list").
    pub object: Option<String>,
    /// List of embedding results.
    pub data: Vec<EmbeddingData>,
}

/// Tuning knobs for embedding requests.
///
/// Use the chainable setter methods to configure, e.g.:
///
/// ```ignore
/// let client = model.create_embedding_client()
///     .dimensions(512);
/// ```
#[derive(Debug, Clone, Default)]
pub struct EmbeddingClientSettings {
    dimensions: Option<u32>,
    encoding_format: Option<String>,
}

impl EmbeddingClientSettings {
    fn serialize(&self, model_id: &str, input: &str) -> Value {
        let mut map = serde_json::Map::new();

        map.insert("model".into(), json!(model_id));
        map.insert("input".into(), json!(input));

        if let Some(dims) = self.dimensions {
            map.insert("dimensions".into(), json!(dims));
        }
        if let Some(ref fmt) = self.encoding_format {
            map.insert("encoding_format".into(), json!(fmt));
        }

        Value::Object(map)
    }
}

/// Client for OpenAI-compatible embedding generation backed by a local model.
pub struct EmbeddingClient {
    model_id: String,
    core: Arc<CoreInterop>,
    settings: EmbeddingClientSettings,
}

impl EmbeddingClient {
    pub(crate) fn new(model_id: &str, core: Arc<CoreInterop>) -> Self {
        Self {
            model_id: model_id.to_owned(),
            core,
            settings: EmbeddingClientSettings::default(),
        }
    }

    /// Set the number of dimensions for the output embeddings.
    pub fn dimensions(mut self, v: u32) -> Self {
        self.settings.dimensions = Some(v);
        self
    }

    /// Set the encoding format ("float" or "base64").
    pub fn encoding_format(mut self, v: impl Into<String>) -> Self {
        self.settings.encoding_format = Some(v.into());
        self
    }

    /// Generate embeddings for the given input text.
    pub async fn generate_embedding(&self, input: &str) -> Result<EmbeddingResponse> {
        Self::validate_input(input)?;

        let request = self.settings.serialize(&self.model_id, input);
        let params = json!({
            "Params": {
                "OpenAICreateRequest": serde_json::to_string(&request)?
            }
        });

        let raw = self
            .core
            .execute_command_async("embeddings".into(), Some(params))
            .await?;
        let parsed: EmbeddingResponse = serde_json::from_str(&raw)?;
        Ok(parsed)
    }

    fn validate_input(input: &str) -> Result<()> {
        if input.trim().is_empty() {
            return Err(crate::error::FoundryLocalError::Validation {
                reason: "input must be a non-empty string".into(),
            });
        }
        Ok(())
    }
}
