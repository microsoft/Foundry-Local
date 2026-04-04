//! OpenAI-compatible chat completions client.

use std::collections::HashMap;
use std::sync::Arc;

use super::chat_types::{
    ChatCompletionRequestMessage, ChatCompletionTools, CreateChatCompletionResponse,
    CreateChatCompletionStreamResponse,
};
use serde::Serialize;
use serde_json::{json, Value};

use crate::detail::core_interop::CoreInterop;
use crate::error::{FoundryLocalError, Result};
use crate::types::{ChatResponseFormat, ChatToolChoice};

use super::json_stream::JsonStream;

/// Tuning knobs for chat completion requests.
///
/// Use the chainable setter methods to configure, e.g.:
///
/// ```ignore
/// let client = model.create_chat_client()
///     .temperature(0.7)
///     .max_tokens(256);
/// ```
#[derive(Debug, Clone, Default, Serialize)]
pub struct ChatClientSettings {
    #[serde(skip_serializing_if = "Option::is_none")]
    frequency_penalty: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    max_tokens: Option<u32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    max_completion_tokens: Option<u32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    n: Option<u32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    temperature: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    presence_penalty: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    top_p: Option<f64>,
    #[serde(skip)]
    top_k: Option<u32>,
    #[serde(skip)]
    random_seed: Option<u64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    response_format: Option<ChatResponseFormat>,
    #[serde(skip_serializing_if = "Option::is_none")]
    tool_choice: Option<ChatToolChoice>,
}

impl ChatClientSettings {
    fn serialize(&self) -> Value {
        let mut value = serde_json::to_value(self).unwrap();
        let map = value.as_object_mut().unwrap();

        // Foundry-specific metadata for settings that don't map directly to
        // the OpenAI spec.
        let mut metadata: HashMap<String, String> = HashMap::new();
        if let Some(k) = self.top_k {
            metadata.insert("top_k".into(), k.to_string());
        }
        if let Some(s) = self.random_seed {
            metadata.insert("random_seed".into(), s.to_string());
        }
        if !metadata.is_empty() {
            map.insert("metadata".into(), json!(metadata));
        }

        value
    }
}

/// A stream of [`CreateChatCompletionStreamResponse`] chunks.
///
/// Returned by [`ChatClient::complete_streaming_chat`].
pub type ChatCompletionStream = JsonStream<CreateChatCompletionStreamResponse>;

/// Client for OpenAI-compatible chat completions backed by a local model.
pub struct ChatClient {
    model_id: String,
    core: Arc<CoreInterop>,
    settings: ChatClientSettings,
}

impl ChatClient {
    pub(crate) fn new(model_id: &str, core: Arc<CoreInterop>) -> Self {
        Self {
            model_id: model_id.to_owned(),
            core,
            settings: ChatClientSettings::default(),
        }
    }

    /// Set the frequency penalty.
    pub fn frequency_penalty(mut self, v: f64) -> Self {
        self.settings.frequency_penalty = Some(v);
        self
    }

    /// Set the maximum number of tokens to generate.
    pub fn max_tokens(mut self, v: u32) -> Self {
        self.settings.max_tokens = Some(v);
        self
    }

    /// Set the maximum number of completion tokens to generate (newer OpenAI field).
    pub fn max_completion_tokens(mut self, v: u32) -> Self {
        self.settings.max_completion_tokens = Some(v);
        self
    }

    /// Set the number of completions to generate.
    pub fn n(mut self, v: u32) -> Self {
        self.settings.n = Some(v);
        self
    }

    /// Set the sampling temperature.
    pub fn temperature(mut self, v: f64) -> Self {
        self.settings.temperature = Some(v);
        self
    }

    /// Set the presence penalty.
    pub fn presence_penalty(mut self, v: f64) -> Self {
        self.settings.presence_penalty = Some(v);
        self
    }

    /// Set the nucleus sampling probability.
    pub fn top_p(mut self, v: f64) -> Self {
        self.settings.top_p = Some(v);
        self
    }

    /// Set the top-k sampling parameter (Foundry extension).
    pub fn top_k(mut self, v: u32) -> Self {
        self.settings.top_k = Some(v);
        self
    }

    /// Set the random seed for reproducible results (Foundry extension).
    pub fn random_seed(mut self, v: u64) -> Self {
        self.settings.random_seed = Some(v);
        self
    }

    /// Set the desired response format.
    pub fn response_format(mut self, v: ChatResponseFormat) -> Self {
        self.settings.response_format = Some(v);
        self
    }

    /// Set the tool choice strategy.
    pub fn tool_choice(mut self, v: ChatToolChoice) -> Self {
        self.settings.tool_choice = Some(v);
        self
    }

    /// Perform a non-streaming chat completion.
    pub async fn complete_chat(
        &self,
        messages: &[ChatCompletionRequestMessage],
        tools: Option<&[ChatCompletionTools]>,
    ) -> Result<CreateChatCompletionResponse> {
        if messages.is_empty() {
            return Err(FoundryLocalError::Validation {
                reason: "messages must be a non-empty array".into(),
            });
        }

        let request = self.build_request(messages, tools, false)?;
        let params = json!({
            "Params": {
                "OpenAICreateRequest": serde_json::to_string(&request)?
            }
        });

        let raw = self
            .core
            .execute_command_async("chat_completions".into(), Some(params))
            .await?;
        let parsed: CreateChatCompletionResponse = serde_json::from_str(&raw)?;
        Ok(parsed)
    }

    /// Perform a streaming chat completion, returning a [`ChatCompletionStream`].
    ///
    /// Use the stream with `futures_core::StreamExt::next()` or
    /// `tokio_stream::StreamExt::next()`.
    pub async fn complete_streaming_chat(
        &self,
        messages: &[ChatCompletionRequestMessage],
        tools: Option<&[ChatCompletionTools]>,
    ) -> Result<ChatCompletionStream> {
        if messages.is_empty() {
            return Err(FoundryLocalError::Validation {
                reason: "messages must be a non-empty array".into(),
            });
        }

        let request = self.build_request(messages, tools, true)?;
        let params = json!({
            "Params": {
                "OpenAICreateRequest": serde_json::to_string(&request)?
            }
        });

        let rx = self
            .core
            .execute_command_streaming_channel("chat_completions".into(), Some(params))
            .await?;

        Ok(ChatCompletionStream::new(rx))
    }

    fn build_request(
        &self,
        messages: &[ChatCompletionRequestMessage],
        tools: Option<&[ChatCompletionTools]>,
        stream: bool,
    ) -> Result<Value> {
        let settings_value = self.settings.serialize();
        let mut map = match settings_value {
            Value::Object(m) => m,
            _ => serde_json::Map::new(),
        };

        map.insert("model".into(), json!(self.model_id));
        map.insert("messages".into(), serde_json::to_value(messages)?);

        if stream {
            map.insert("stream".into(), json!(true));
        }

        if let Some(t) = tools {
            map.insert("tools".into(), serde_json::to_value(t)?);
        }

        Ok(Value::Object(map))
    }
}
