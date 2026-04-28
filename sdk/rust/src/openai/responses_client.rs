//! HTTP client for the OpenAI Responses API.

use std::collections::HashMap;
use std::pin::Pin;
use std::task::{Context, Poll};
use std::time::Duration;

use async_stream::try_stream;
use bytes::Bytes;
use futures_core::Stream;
use reqwest::Client;
use serde_json::Value;

use crate::error::{FoundryLocalError, Result};

use super::responses_types::{
    DeleteResponseResult, FunctionToolDefinition, InputItemsListResponse, ListResponsesResult,
    ReasoningConfig, ResponseCreateRequest, ResponseInput, ResponseObject, StreamingEvent,
    TextConfig,
};

// ============================================================================
// Settings
// ============================================================================

/// Configuration applied to every request made by a [`ResponsesClient`].
///
/// Use the public fields to set defaults; individual calls can override them
/// via the `options` parameter.
#[derive(Debug, Clone)]
pub struct ResponsesClientSettings {
    pub instructions: Option<String>,
    pub temperature: Option<f32>,
    pub top_p: Option<f32>,
    pub max_output_tokens: Option<u32>,
    pub frequency_penalty: Option<f32>,
    pub presence_penalty: Option<f32>,
    /// Tool choice strategy (serialised as JSON).
    pub tool_choice: Option<Value>,
    /// Truncation strategy: `"auto"` or `"disabled"`.
    pub truncation: Option<String>,
    pub parallel_tool_calls: Option<bool>,
    /// Whether to persist the response for later retrieval.
    ///
    /// Defaults to `None`, which omits the field and lets the server decide.
    pub store: Option<bool>,
    pub metadata: Option<HashMap<String, String>>,
    pub reasoning: Option<ReasoningConfig>,
    pub text: Option<TextConfig>,
    pub seed: Option<u32>,
    /// Request timeout used for non-streaming calls; streaming calls use this as
    /// a connect timeout so long-running streams are not cut off mid-response.
    pub timeout: Duration,
}

impl Default for ResponsesClientSettings {
    fn default() -> Self {
        Self {
            store: None,
            instructions: None,
            temperature: None,
            top_p: None,
            max_output_tokens: None,
            frequency_penalty: None,
            presence_penalty: None,
            tool_choice: None,
            truncation: None,
            parallel_tool_calls: None,
            metadata: None,
            reasoning: None,
            text: None,
            seed: None,
            timeout: Duration::from_secs(60),
        }
    }
}

impl ResponsesClientSettings {
    /// Create settings with sensible defaults.
    pub fn new() -> Self {
        Self::default()
    }
}

// ============================================================================
// SSE Stream
// ============================================================================

/// A stream of [`StreamingEvent`]s parsed from a Server-Sent Events response body.
pub struct SseStream {
    inner: Pin<Box<dyn Stream<Item = Result<StreamingEvent>> + Send>>,
}

impl Stream for SseStream {
    type Item = Result<StreamingEvent>;

    fn poll_next(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Option<Self::Item>> {
        self.inner.as_mut().poll_next(cx)
    }
}

// ============================================================================
// Client
// ============================================================================

/// Client for the OpenAI Responses API served by Foundry Local's embedded web service.
///
/// Unlike the chat/audio/embedding clients (which use FFI via CoreInterop),
/// this client is HTTP-only and communicates directly with the embedded web service.
///
/// # Example
/// ```ignore
/// let manager = FoundryLocalManager::create(config)?;
/// manager.start_web_service().await?;
/// let client = manager.get_responses_client(Some("my-model-id"))?;
///
/// // Non-streaming
/// let response = client.create(ResponseInput::Text("Hello!".into()), None).await?;
/// println!("{}", response.output_text());
///
/// // Streaming
/// use tokio_stream::StreamExt;
/// let mut stream = client.create_streaming(ResponseInput::Text("Tell me a story".into()), None).await?;
/// while let Some(event) = stream.next().await {
///     if let Ok(StreamingEvent::OutputTextDelta { delta, .. }) = event {
///         print!("{delta}");
///     }
/// }
/// ```
pub struct ResponsesClient {
    http: Client,
    base_url: String,
    model_id: Option<String>,
    /// Shared settings applied to every request. Modify via `client.settings`.
    pub settings: ResponsesClientSettings,
}

impl ResponsesClient {
    /// Create a new [`ResponsesClient`].
    ///
    /// - `base_url` — base URL of the Foundry Local web service (e.g. `"http://127.0.0.1:5273"`).
    ///   Trailing slashes are stripped.
    /// - `model_id` — default model used when not specified per-request.
    pub fn new(base_url: &str, model_id: Option<&str>) -> Self {
        let base_url = base_url.trim_end_matches('/').to_owned();
        Self {
            http: Client::new(),
            base_url,
            model_id: model_id.map(str::to_owned),
            settings: ResponsesClientSettings::default(),
        }
    }

    // ── Public API ───────────────────────────────────────────────────────────

    /// Create a model response (non-streaming).
    ///
    /// Settings are merged in order: `model + input` → `self.settings` → `options`.
    pub async fn create(
        &self,
        input: ResponseInput,
        options: Option<ResponseCreateRequest>,
    ) -> Result<ResponseObject> {
        self.validate_input(&input)?;
        if let Some(ref opts) = options {
            self.validate_tools(opts.tools.as_deref())?;
        }

        let body = self.build_request(input, options, false)?;
        let resp = self
            .http
            .post(self.url("/v1/responses"))
            .timeout(self.request_timeout()?)
            .json(&body)
            .send()
            .await?;

        self.parse_json_response(resp).await
    }

    /// Create a model response with streaming via Server-Sent Events.
    ///
    /// Returns an `impl Stream<Item = Result<StreamingEvent>>` that yields parsed
    /// events as they arrive.  Use `tokio_stream::StreamExt` to iterate:
    ///
    /// ```ignore
    /// use tokio_stream::StreamExt;
    /// let mut stream = client.create_streaming(input, None).await?;
    /// while let Some(event) = stream.next().await {
    ///     // …
    /// }
    /// ```
    pub async fn create_streaming(
        &self,
        input: ResponseInput,
        options: Option<ResponseCreateRequest>,
    ) -> Result<SseStream> {
        self.validate_input(&input)?;
        if let Some(ref opts) = options {
            self.validate_tools(opts.tools.as_deref())?;
        }

        let body = self.build_request(input, options, true)?;
        let http = Client::builder()
            .connect_timeout(self.request_timeout()?)
            .build()?;
        let resp = http
            .post(self.url("/v1/responses"))
            .header("Accept", "text/event-stream")
            .json(&body)
            .send()
            .await?;

        if !resp.status().is_success() {
            let status = resp.status();
            let text = resp.text().await.unwrap_or_else(|_| status.to_string());
            return Err(FoundryLocalError::Validation {
                reason: format!("Responses API error ({status}): {text}"),
            });
        }

        let byte_stream = resp.bytes_stream();
        let parsed = parse_sse_stream(byte_stream);
        Ok(SseStream {
            inner: Box::pin(parsed),
        })
    }

    /// Retrieve a stored response by ID.
    pub async fn get(&self, response_id: &str) -> Result<ResponseObject> {
        self.validate_id(response_id, "response_id")?;
        let url = self.url(&format!(
            "/v1/responses/{}",
            urlencoding::encode(response_id)
        ));
        let resp = self
            .http
            .get(url)
            .timeout(self.request_timeout()?)
            .send()
            .await?;
        self.parse_json_response(resp).await
    }

    /// Delete a stored response by ID.
    pub async fn delete(&self, response_id: &str) -> Result<DeleteResponseResult> {
        self.validate_id(response_id, "response_id")?;
        let url = self.url(&format!(
            "/v1/responses/{}",
            urlencoding::encode(response_id)
        ));
        let resp = self
            .http
            .delete(url)
            .timeout(self.request_timeout()?)
            .send()
            .await?;
        self.parse_json_response(resp).await
    }

    /// Cancel an in-progress response.
    pub async fn cancel(&self, response_id: &str) -> Result<ResponseObject> {
        self.validate_id(response_id, "response_id")?;
        let url = self.url(&format!(
            "/v1/responses/{}/cancel",
            urlencoding::encode(response_id)
        ));
        let resp = self
            .http
            .post(url)
            .timeout(self.request_timeout()?)
            .send()
            .await?;
        self.parse_json_response(resp).await
    }

    /// Retrieve the input items for a stored response.
    pub async fn get_input_items(&self, response_id: &str) -> Result<InputItemsListResponse> {
        self.validate_id(response_id, "response_id")?;
        let url = self.url(&format!(
            "/v1/responses/{}/input_items",
            urlencoding::encode(response_id)
        ));
        let resp = self
            .http
            .get(url)
            .timeout(self.request_timeout()?)
            .send()
            .await?;
        self.parse_json_response(resp).await
    }

    /// List all stored responses (extension endpoint).
    pub async fn list(&self) -> Result<ListResponsesResult> {
        self.list_with_options(None).await
    }

    /// List stored responses with optional pagination controls.
    pub async fn list_with_options(
        &self,
        options: Option<&super::responses_types::ListResponsesOptions>,
    ) -> Result<ListResponsesResult> {
        let mut req = self.http.get(self.url("/v1/responses"));
        if let Some(options) = options {
            let mut query = Vec::new();
            if let Some(limit) = options.limit {
                if limit == 0 {
                    return Err(FoundryLocalError::Validation {
                        reason: "list limit must be greater than zero.".into(),
                    });
                }
                query.push(("limit", limit.to_string()));
            }
            if let Some(order) = &options.order {
                if order != "asc" && order != "desc" {
                    return Err(FoundryLocalError::Validation {
                        reason: "list order must be either \"asc\" or \"desc\".".into(),
                    });
                }
                query.push(("order", order.clone()));
            }
            if let Some(after) = &options.after {
                self.validate_id(after, "after")?;
                query.push(("after", after.clone()));
            }
            req = req.query(&query);
        }
        let resp = req.timeout(self.request_timeout()?).send().await?;
        self.parse_json_response(resp).await
    }

    // ── Private helpers ──────────────────────────────────────────────────────

    fn url(&self, path: &str) -> String {
        format!("{}{}", self.base_url, path)
    }

    /// Merge `input`, `self.settings`, and caller `options` into a single
    /// `ResponseCreateRequest`.
    fn build_request(
        &self,
        input: ResponseInput,
        options: Option<ResponseCreateRequest>,
        stream: bool,
    ) -> Result<ResponseCreateRequest> {
        // Determine model: options override self.model_id
        let model = options
            .as_ref()
            .map(|o| o.model.clone())
            .filter(|m| !m.trim().is_empty())
            .or_else(|| self.model_id.clone())
            .ok_or_else(|| FoundryLocalError::Validation {
                reason: "Model must be specified in the constructor or via options.model.".into(),
            })?;

        // Start with settings defaults
        let s = &self.settings;

        let mut req = ResponseCreateRequest {
            model,
            input,
            stream: Some(stream),
            // Settings defaults
            instructions: s.instructions.clone(),
            temperature: s.temperature,
            top_p: s.top_p,
            max_output_tokens: s.max_output_tokens,
            frequency_penalty: s.frequency_penalty,
            presence_penalty: s.presence_penalty,
            tool_choice: s.tool_choice.clone(),
            truncation: s.truncation.clone(),
            parallel_tool_calls: s.parallel_tool_calls,
            store: s.store,
            metadata: s.metadata.clone(),
            reasoning: s.reasoning.clone(),
            text: s.text.clone(),
            seed: s.seed,
            // Not in settings
            previous_response_id: None,
            tools: None,
            user: None,
        };

        // Apply per-call overrides
        if let Some(opts) = options {
            if !opts.model.trim().is_empty() {
                req.model = opts.model;
            }
            // Only override input if the caller passed an options object with explicit input;
            // in practice options.input will always be overwritten by the positional `input`.
            if let Some(v) = opts.instructions {
                req.instructions = Some(v);
            }
            if let Some(v) = opts.previous_response_id {
                req.previous_response_id = Some(v);
            }
            if let Some(v) = opts.tools {
                req.tools = Some(v);
            }
            if let Some(v) = opts.tool_choice {
                req.tool_choice = Some(v);
            }
            if let Some(v) = opts.temperature {
                req.temperature = Some(v);
            }
            if let Some(v) = opts.top_p {
                req.top_p = Some(v);
            }
            if let Some(v) = opts.max_output_tokens {
                req.max_output_tokens = Some(v);
            }
            if let Some(v) = opts.frequency_penalty {
                req.frequency_penalty = Some(v);
            }
            if let Some(v) = opts.presence_penalty {
                req.presence_penalty = Some(v);
            }
            if let Some(v) = opts.seed {
                req.seed = Some(v);
            }
            if let Some(v) = opts.truncation {
                req.truncation = Some(v);
            }
            if let Some(v) = opts.parallel_tool_calls {
                req.parallel_tool_calls = Some(v);
            }
            if let Some(v) = opts.store {
                req.store = Some(v);
            }
            if let Some(v) = opts.metadata {
                req.metadata = Some(v);
            }
            if let Some(v) = opts.user {
                req.user = Some(v);
            }
            if let Some(v) = opts.reasoning {
                req.reasoning = Some(v);
            }
            if let Some(v) = opts.text {
                req.text = Some(v);
            }
        }

        Ok(req)
    }

    fn validate_input(&self, input: &ResponseInput) -> Result<()> {
        match input {
            ResponseInput::Text(s) if s.trim().is_empty() => Err(FoundryLocalError::Validation {
                reason: "Input string cannot be empty.".into(),
            }),
            ResponseInput::Items(items) if items.is_empty() => Err(FoundryLocalError::Validation {
                reason: "Input items array cannot be empty.".into(),
            }),
            ResponseInput::Items(items) => {
                for item in items {
                    Self::validate_response_item(item)?;
                }
                Ok(())
            }
            _ => Ok(()),
        }
    }

    fn validate_response_item(item: &super::responses_types::ResponseItem) -> Result<()> {
        match item {
            super::responses_types::ResponseItem::Message {
                content: super::responses_types::MessageContent::Parts(parts),
                ..
            } => {
                for part in parts {
                    Self::validate_content_part(part)?;
                }
            }
            super::responses_types::ResponseItem::Reasoning {
                content: Some(parts),
                ..
            } => {
                for part in parts {
                    Self::validate_content_part(part)?;
                }
            }
            _ => {}
        }
        Ok(())
    }

    fn validate_content_part(part: &super::responses_types::ContentPart) -> Result<()> {
        if let super::responses_types::ContentPart::InputImage {
            image_url,
            image_data,
            ..
        } = part
        {
            let has_image_url = image_url.as_ref().is_some_and(|v| !v.trim().is_empty());
            let has_image_data = image_data.as_ref().is_some_and(|v| !v.trim().is_empty());
            if has_image_url == has_image_data {
                return Err(FoundryLocalError::Validation {
                    reason:
                        "Provide exactly one of image_url or image_data for input_image content."
                            .into(),
                });
            }
        }
        Ok(())
    }

    fn validate_tools(&self, tools: Option<&[FunctionToolDefinition]>) -> Result<()> {
        let Some(tools) = tools else {
            return Ok(());
        };
        for tool in tools {
            if tool.tool_type != "function" {
                return Err(FoundryLocalError::Validation {
                    reason: format!(
                        "Each tool must have type \"function\", got \"{}\".",
                        tool.tool_type
                    ),
                });
            }
            if tool.name.trim().is_empty() {
                return Err(FoundryLocalError::Validation {
                    reason: "Each tool must have a non-empty \"name\".".into(),
                });
            }
        }
        Ok(())
    }

    fn validate_id(&self, id: &str, param: &str) -> Result<()> {
        if id.trim().is_empty() {
            return Err(FoundryLocalError::Validation {
                reason: format!("{param} must be a non-empty string."),
            });
        }
        // OpenAI does not publish a max ID length; keep this aligned with the
        // JS SDK to avoid surprising client-side rejections of valid server IDs.
        if id.len() > 1024 {
            return Err(FoundryLocalError::Validation {
                reason: format!("{param} exceeds maximum length (1024)."),
            });
        }
        Ok(())
    }

    fn request_timeout(&self) -> Result<Duration> {
        if self.settings.timeout.is_zero() {
            return Err(FoundryLocalError::Validation {
                reason: "ResponsesClientSettings.timeout must be greater than zero.".into(),
            });
        }
        Ok(self.settings.timeout)
    }

    async fn parse_json_response<T>(&self, resp: reqwest::Response) -> Result<T>
    where
        T: serde::de::DeserializeOwned,
    {
        let status = resp.status();
        let text = resp.text().await?;
        if !status.is_success() {
            return Err(FoundryLocalError::Validation {
                reason: format!("Responses API error ({status}): {text}"),
            });
        }
        serde_json::from_str(&text).map_err(FoundryLocalError::from)
    }
}

// ============================================================================
// SSE parser
// ============================================================================

/// Parse a raw bytes stream (from `reqwest`) as Server-Sent Events.
///
/// Each complete SSE block (`\n\n`-separated) is parsed into a [`StreamingEvent`].
/// The stream ends on `data: [DONE]` or when the source is exhausted.
fn parse_sse_stream<S>(byte_stream: S) -> impl Stream<Item = Result<StreamingEvent>> + Send
where
    S: Stream<Item = std::result::Result<Bytes, reqwest::Error>> + Send + 'static,
{
    try_stream! {
        use tokio_stream::StreamExt as _;

        let mut byte_stream = std::pin::pin!(byte_stream);
        // Buffer accumulates bytes until we have complete SSE blocks.
        let mut buf = String::new();

        while let Some(chunk) = byte_stream.next().await {
            let bytes: Bytes = chunk.map_err(FoundryLocalError::from)?;
            // SSE is always UTF-8
            let text = std::str::from_utf8(&bytes).map_err(|e| FoundryLocalError::Validation {
                reason: format!("SSE stream contained invalid UTF-8: {e}"),
            })?;
            buf.push_str(text);

            // Process all complete SSE blocks (separated by double newlines).
            loop {
                let Some(block_end) = buf.find("\n\n") else {
                    break;
                };
                let block = buf[..block_end].to_owned();
                buf = buf[block_end + 2..].to_owned();

                let trimmed = block.trim();
                if trimmed.is_empty() {
                    continue;
                }

                // Terminal signal
                if trimmed == "data: [DONE]" {
                    return;
                }

                // Collect `data:` lines (per SSE spec, multiple are joined with \n)
                let mut data_lines: Vec<&str> = Vec::new();
                for line in trimmed.split('\n') {
                    if let Some(rest) = line.strip_prefix("data: ") {
                        data_lines.push(rest);
                    } else if line == "data:" {
                        data_lines.push("");
                    }
                    // `event:` lines are informational; the type lives inside the JSON.
                }

                if data_lines.is_empty() {
                    continue;
                }

                let event_json = data_lines.join("\n");
                let event: StreamingEvent =
                    serde_json::from_str(&event_json).map_err(FoundryLocalError::from)?;
                yield event;
            }
        }
    }
}
