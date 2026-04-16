//! OpenAI-compatible embedding client.

use std::sync::Arc;

use async_openai::types::embeddings::CreateEmbeddingResponse;
use serde_json::{json, Value};

use crate::detail::core_interop::CoreInterop;
use crate::error::{FoundryLocalError, Result};

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
    fn serialize(&self) -> Value {
        let mut map = serde_json::Map::new();

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

    /// Generate embeddings for a single input text.
    pub async fn generate_embedding(&self, input: &str) -> Result<CreateEmbeddingResponse> {
        Self::validate_input(input)?;
        let request = self.build_request(json!(input))?;
        self.execute_request(request).await
    }

    /// Generate embeddings for multiple input texts in a single request.
    pub async fn generate_embeddings(&self, inputs: &[&str]) -> Result<CreateEmbeddingResponse> {
        if inputs.is_empty() {
            return Err(FoundryLocalError::Validation {
                reason: "inputs must be a non-empty array".into(),
            });
        }
        for input in inputs {
            Self::validate_input(input)?;
        }
        let request = self.build_request(json!(inputs))?;
        self.execute_request(request).await
    }

    async fn execute_request(&self, request: Value) -> Result<CreateEmbeddingResponse> {
        let params = json!({
            "Params": {
                "OpenAICreateRequest": serde_json::to_string(&request)?
            }
        });

        let raw = self
            .core
            .execute_command_async("embeddings".into(), Some(params))
            .await?;

        // Patch the response to add fields required by async_openai types
        // that the server doesn't return (object on each item, usage)
        let mut response_value: Value = serde_json::from_str(&raw)?;
        if let Some(data) = response_value
            .get_mut("data")
            .and_then(|d| d.as_array_mut())
        {
            for item in data {
                if item.get("object").is_none() {
                    item.as_object_mut()
                        .map(|m| m.insert("object".into(), json!("embedding")));
                }
            }
        }
        if response_value.get("usage").is_none() {
            response_value.as_object_mut().map(|m| {
                m.insert(
                    "usage".into(),
                    json!({"prompt_tokens": 0, "total_tokens": 0}),
                )
            });
        }

        let parsed: CreateEmbeddingResponse = serde_json::from_value(response_value)?;
        Ok(parsed)
    }

    fn build_request(&self, input: Value) -> Result<Value> {
        Self::validate_encoding_format(&self.settings.encoding_format)?;

        let settings_value = self.settings.serialize();
        let mut map = match settings_value {
            Value::Object(m) => m,
            _ => serde_json::Map::new(),
        };

        map.insert("model".into(), json!(self.model_id));
        map.insert("input".into(), input);

        Ok(Value::Object(map))
    }

    fn validate_encoding_format(format: &Option<String>) -> Result<()> {
        if let Some(ref fmt) = format {
            let valid = ["float", "base64"];
            if !valid.contains(&fmt.as_str()) {
                return Err(FoundryLocalError::Validation {
                    reason: format!("encoding_format must be one of: {}", valid.join(", ")),
                });
            }
        }
        Ok(())
    }

    fn validate_input(input: &str) -> Result<()> {
        if input.trim().is_empty() {
            return Err(FoundryLocalError::Validation {
                reason: "input must be a non-empty string".into(),
            });
        }
        Ok(())
    }
}
