//! OpenAI-compatible chat completions client.

use std::collections::HashMap;
use std::pin::Pin;
use std::sync::Arc;
use std::task::{Context, Poll};

use futures_core::Future;

use async_openai::types::chat::{
    ChatCompletionRequestMessage, ChatCompletionTools, CreateChatCompletionResponse,
    CreateChatCompletionStreamResponse,
};
use serde_json::{json, Value};

use crate::detail::core_interop::CoreInterop;
use crate::error::{FoundryLocalError, Result};
use crate::types::{ChatResponseFormat, ChatToolChoice};

/// Tuning knobs for chat completion requests.
///
/// Use the chainable setter methods to configure, e.g.:
///
/// ```ignore
/// let mut client = model.create_chat_client();
/// client.temperature(0.7).max_tokens(256);
/// ```
#[derive(Debug, Clone, Default)]
pub struct ChatClientSettings {
    frequency_penalty: Option<f64>,
    max_tokens: Option<u32>,
    n: Option<u32>,
    temperature: Option<f64>,
    presence_penalty: Option<f64>,
    top_p: Option<f64>,
    top_k: Option<u32>,
    random_seed: Option<u64>,
    response_format: Option<ChatResponseFormat>,
    tool_choice: Option<ChatToolChoice>,
}

impl ChatClientSettings {
    /// Serialise settings into the JSON fragment expected by the native core.
    fn serialize(&self) -> Value {
        let mut map = serde_json::Map::new();

        if let Some(v) = self.frequency_penalty {
            map.insert("frequency_penalty".into(), json!(v));
        }
        if let Some(v) = self.max_tokens {
            map.insert("max_tokens".into(), json!(v));
        }
        if let Some(v) = self.n {
            map.insert("n".into(), json!(v));
        }
        if let Some(v) = self.presence_penalty {
            map.insert("presence_penalty".into(), json!(v));
        }
        if let Some(v) = self.temperature {
            map.insert("temperature".into(), json!(v));
        }
        if let Some(v) = self.top_p {
            map.insert("top_p".into(), json!(v));
        }

        if let Some(ref rf) = self.response_format {
            let mut rf_map = serde_json::Map::new();
            match rf {
                ChatResponseFormat::Text => {
                    rf_map.insert("type".into(), json!("text"));
                }
                ChatResponseFormat::JsonObject => {
                    rf_map.insert("type".into(), json!("json_object"));
                }
                ChatResponseFormat::JsonSchema(schema) => {
                    rf_map.insert("type".into(), json!("json_schema"));
                    rf_map.insert("jsonSchema".into(), json!(schema));
                }
                ChatResponseFormat::LarkGrammar(grammar) => {
                    rf_map.insert("type".into(), json!("lark_grammar"));
                    rf_map.insert("larkGrammar".into(), json!(grammar));
                }
            }
            map.insert("response_format".into(), Value::Object(rf_map));
        }

        if let Some(ref tc) = self.tool_choice {
            let mut tc_map = serde_json::Map::new();
            match tc {
                ChatToolChoice::None => {
                    tc_map.insert("type".into(), json!("none"));
                }
                ChatToolChoice::Auto => {
                    tc_map.insert("type".into(), json!("auto"));
                }
                ChatToolChoice::Required => {
                    tc_map.insert("type".into(), json!("required"));
                }
                ChatToolChoice::Function(name) => {
                    tc_map.insert("type".into(), json!("function"));
                    tc_map.insert("name".into(), json!(name));
                }
            }
            map.insert("tool_choice".into(), Value::Object(tc_map));
        }

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

        Value::Object(map)
    }
}

/// A stream of [`CreateChatCompletionStreamResponse`] chunks.
///
/// Returned by [`ChatClient::complete_streaming_chat`].
pub struct ChatCompletionStream {
    rx: tokio::sync::mpsc::UnboundedReceiver<String>,
    handle: Option<tokio::task::JoinHandle<Result<String>>>,
}

impl futures_core::Stream for ChatCompletionStream {
    type Item = Result<CreateChatCompletionStreamResponse>;

    fn poll_next(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Option<Self::Item>> {
        match self.rx.poll_recv(cx) {
            Poll::Ready(Some(chunk)) => {
                if chunk.is_empty() {
                    // Skip empty chunks and poll again.
                    cx.waker().wake_by_ref();
                    Poll::Pending
                } else {
                    let parsed = serde_json::from_str::<CreateChatCompletionStreamResponse>(&chunk)
                        .map_err(FoundryLocalError::from);
                    Poll::Ready(Some(parsed))
                }
            }
            Poll::Ready(None) => {
                // Channel closed — check the JoinHandle for FFI errors that
                // would otherwise be swallowed by tokio.
                if let Some(handle) = self.handle.as_mut() {
                    match Pin::new(handle).poll(cx) {
                        Poll::Ready(Ok(Ok(_))) => {
                            self.handle.take();
                            Poll::Ready(None)
                        }
                        Poll::Ready(Ok(Err(e))) => {
                            self.handle.take();
                            Poll::Ready(Some(Err(e)))
                        }
                        Poll::Ready(Err(e)) => {
                            self.handle.take();
                            Poll::Ready(Some(Err(FoundryLocalError::CommandExecution {
                                reason: format!("task join error: {e}"),
                            })))
                        }
                        Poll::Pending => Poll::Pending,
                    }
                } else {
                    Poll::Ready(None)
                }
            }
            Poll::Pending => Poll::Pending,
        }
    }
}

impl ChatCompletionStream {
    /// Consume the stream and wait for the background FFI task to finish.
    ///
    /// Call this after the stream is exhausted to retrieve any error from
    /// the native core response buffer.
    pub async fn close(mut self) -> Result<()> {
        if let Some(handle) = self.handle.take() {
            handle
                .await
                .map_err(|e| FoundryLocalError::CommandExecution {
                    reason: format!("task join error: {e}"),
                })?
                .map(|_| ())
        } else {
            Ok(())
        }
    }
}

/// Client for OpenAI-compatible chat completions backed by a local model.
pub struct ChatClient {
    model_id: String,
    core: Arc<CoreInterop>,
    settings: ChatClientSettings,
}

impl ChatClient {
    pub(crate) fn new(model_id: String, core: Arc<CoreInterop>) -> Self {
        Self {
            model_id,
            core,
            settings: ChatClientSettings::default(),
        }
    }

    /// Set the frequency penalty.
    pub fn frequency_penalty(&mut self, v: f64) -> &mut Self {
        self.settings.frequency_penalty = Some(v);
        self
    }

    /// Set the maximum number of tokens to generate.
    pub fn max_tokens(&mut self, v: u32) -> &mut Self {
        self.settings.max_tokens = Some(v);
        self
    }

    /// Set the number of completions to generate.
    pub fn n(&mut self, v: u32) -> &mut Self {
        self.settings.n = Some(v);
        self
    }

    /// Set the sampling temperature.
    pub fn temperature(&mut self, v: f64) -> &mut Self {
        self.settings.temperature = Some(v);
        self
    }

    /// Set the presence penalty.
    pub fn presence_penalty(&mut self, v: f64) -> &mut Self {
        self.settings.presence_penalty = Some(v);
        self
    }

    /// Set the nucleus sampling probability.
    pub fn top_p(&mut self, v: f64) -> &mut Self {
        self.settings.top_p = Some(v);
        self
    }

    /// Set the top-k sampling parameter (Foundry extension).
    pub fn top_k(&mut self, v: u32) -> &mut Self {
        self.settings.top_k = Some(v);
        self
    }

    /// Set the random seed for reproducible results (Foundry extension).
    pub fn random_seed(&mut self, v: u64) -> &mut Self {
        self.settings.random_seed = Some(v);
        self
    }

    /// Set the desired response format.
    pub fn response_format(&mut self, v: ChatResponseFormat) -> &mut Self {
        self.settings.response_format = Some(v);
        self
    }

    /// Set the tool choice strategy.
    pub fn tool_choice(&mut self, v: ChatToolChoice) -> &mut Self {
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

        let (rx, handle) = self
            .core
            .execute_command_streaming_channel("chat_completions".into(), Some(params))
            .await?;

        Ok(ChatCompletionStream {
            rx,
            handle: Some(handle),
        })
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
