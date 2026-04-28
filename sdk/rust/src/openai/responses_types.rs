//! Type definitions for the OpenAI Responses API.

use std::collections::HashMap;

use serde::{Deserialize, Serialize};
use serde_json::Value;

// ============================================================================
// Content Parts
// ============================================================================

/// An annotation attached to an output-text content part.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Annotation {
    #[serde(rename = "type")]
    pub annotation_type: String,
    pub start_index: u32,
    pub end_index: u32,
    /// URL for url_citation annotations.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub url: Option<String>,
    /// Title for url_citation annotations.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub title: Option<String>,
}

/// Log probability for a token.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LogProb {
    pub token: String,
    pub logprob: f64,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub bytes: Option<Vec<u8>>,
}

/// A content part within a message or response.
///
/// Discriminated on the `"type"` field.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type")]
pub enum ContentPart {
    /// Plain text input content.
    #[serde(rename = "input_text")]
    InputText { text: String },

    /// Image input content (vision).
    ///
    /// This models Foundry Local's server contract. The server accepts either
    /// `image_url` or `image_data`; when `image_data` is used, `media_type` lets
    /// the server build the underlying data URI. If omitted, the server may infer
    /// the media type.
    #[serde(rename = "input_image")]
    InputImage {
        /// URL of the image (mutually exclusive with `image_data`).
        #[serde(skip_serializing_if = "Option::is_none")]
        image_url: Option<String>,
        /// Base64-encoded image bytes (mutually exclusive with `image_url`).
        #[serde(skip_serializing_if = "Option::is_none")]
        image_data: Option<String>,
        /// MIME type of the image, e.g. `"image/png"`.
        #[serde(skip_serializing_if = "Option::is_none")]
        media_type: Option<String>,
        /// Detail level: `"low"`, `"high"`, or `"auto"`.
        #[serde(skip_serializing_if = "Option::is_none")]
        detail: Option<String>,
    },

    /// File input content.
    #[serde(rename = "input_file")]
    InputFile { filename: String, file_url: String },

    /// Text produced by the model.
    #[serde(rename = "output_text")]
    OutputText {
        text: String,
        #[serde(skip_serializing_if = "Option::is_none")]
        annotations: Option<Vec<Annotation>>,
        #[serde(skip_serializing_if = "Option::is_none")]
        logprobs: Option<Vec<LogProb>>,
    },

    /// Model refusal.
    #[serde(rename = "refusal")]
    Refusal { refusal: String },
}

// ============================================================================
// Message Content (string | ContentPart[])
// ============================================================================

/// The content of a message item — either a plain string or a list of content parts.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(untagged)]
pub enum MessageContent {
    Text(String),
    Parts(Vec<ContentPart>),
}

// ============================================================================
// Response Items
// ============================================================================

/// An item in a request or response — discriminated on `"type"`.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type")]
pub enum ResponseItem {
    #[serde(rename = "message")]
    Message {
        #[serde(skip_serializing_if = "Option::is_none")]
        id: Option<String>,
        role: String,
        content: MessageContent,
        #[serde(skip_serializing_if = "Option::is_none")]
        status: Option<String>,
    },

    #[serde(rename = "function_call")]
    FunctionCall {
        #[serde(skip_serializing_if = "Option::is_none")]
        id: Option<String>,
        call_id: String,
        name: String,
        arguments: String,
        #[serde(skip_serializing_if = "Option::is_none")]
        status: Option<String>,
    },

    #[serde(rename = "function_call_output")]
    FunctionCallOutput {
        #[serde(skip_serializing_if = "Option::is_none")]
        id: Option<String>,
        call_id: String,
        output: String,
        #[serde(skip_serializing_if = "Option::is_none")]
        status: Option<String>,
    },

    #[serde(rename = "item_reference")]
    ItemReference { id: String },

    #[serde(rename = "reasoning")]
    Reasoning {
        #[serde(skip_serializing_if = "Option::is_none")]
        id: Option<String>,
        #[serde(skip_serializing_if = "Option::is_none")]
        content: Option<Vec<ContentPart>>,
        #[serde(skip_serializing_if = "Option::is_none")]
        encrypted_content: Option<String>,
        #[serde(skip_serializing_if = "Option::is_none")]
        summary: Option<String>,
        #[serde(skip_serializing_if = "Option::is_none")]
        status: Option<String>,
    },
}

// ============================================================================
// Response Input
// ============================================================================

/// The `input` field of a [`ResponseCreateRequest`]: either a plain string prompt
/// or a structured list of [`ResponseItem`]s.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(untagged)]
pub enum ResponseInput {
    Text(String),
    Items(Vec<ResponseItem>),
}

// ============================================================================
// Tool Definitions
// ============================================================================

/// A function tool definition passed to the model.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FunctionToolDefinition {
    /// Always `"function"`.
    #[serde(rename = "type")]
    pub tool_type: String,
    pub name: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub description: Option<String>,
    /// JSON Schema for the function parameters.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub parameters: Option<Value>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub strict: Option<bool>,
}

// ============================================================================
// Text & Reasoning Config
// ============================================================================

/// Format constraints for model text output (constrained generation).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TextFormat {
    /// `"text"`, `"json_object"`, `"json_schema"`, `"lark_grammar"`, or `"regex"`.
    #[serde(rename = "type")]
    pub format_type: String,
    /// Schema name (for `json_schema`).
    #[serde(skip_serializing_if = "Option::is_none")]
    pub name: Option<String>,
    /// Schema description (for `json_schema`).
    #[serde(skip_serializing_if = "Option::is_none")]
    pub description: Option<String>,
    /// JSON Schema object (for `json_schema`).
    #[serde(skip_serializing_if = "Option::is_none")]
    pub schema: Option<Value>,
    /// Strict mode (for `json_schema`).
    #[serde(skip_serializing_if = "Option::is_none")]
    pub strict: Option<bool>,
}

/// Text output configuration.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TextConfig {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub format: Option<TextFormat>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub verbosity: Option<String>,
}

/// Reasoning configuration for reasoning-capable models.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ReasoningConfig {
    /// Effort level: `"low"`, `"medium"`, or `"high"`.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub effort: Option<String>,
    /// Summary style: `"auto"`, `"concise"`, or `"detailed"`.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub summary: Option<String>,
}

// ============================================================================
// Request
// ============================================================================

/// Request body for `POST /v1/responses`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ResponseCreateRequest {
    pub model: String,
    pub input: ResponseInput,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub instructions: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub previous_response_id: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub tools: Option<Vec<FunctionToolDefinition>>,
    /// `"none"` | `"auto"` | `"required"` | `{ "type": "function", "name": "..." }`.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub tool_choice: Option<Value>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub stream: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub store: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub temperature: Option<f32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub top_p: Option<f32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub max_output_tokens: Option<u32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub frequency_penalty: Option<f32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub presence_penalty: Option<f32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub seed: Option<u32>,
    /// `"auto"` or `"disabled"`.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub truncation: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub parallel_tool_calls: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub metadata: Option<HashMap<String, String>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub user: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub reasoning: Option<ReasoningConfig>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub text: Option<TextConfig>,
}

// ============================================================================
// Response Object
// ============================================================================

/// Usage statistics attached to a completed response.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ResponseUsage {
    pub input_tokens: u32,
    pub output_tokens: u32,
    pub total_tokens: u32,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub input_tokens_details: Option<InputTokensDetails>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub output_tokens_details: Option<OutputTokensDetails>,
}

/// Details about input token counts.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct InputTokensDetails {
    pub cached_tokens: u32,
}

/// Details about output token counts.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OutputTokensDetails {
    pub reasoning_tokens: u32,
}

/// An error payload inside a response object.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ResponseError {
    pub code: String,
    pub message: String,
}

/// Optional details about why a response is incomplete.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct IncompleteDetails {
    pub reason: String,
}

/// A completed (or failed) response from the Responses API.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ResponseObject {
    pub id: String,
    pub object: String,
    pub created_at: i64,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub completed_at: Option<i64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub failed_at: Option<i64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub cancelled_at: Option<i64>,
    /// `"queued"`, `"in_progress"`, `"completed"`, `"failed"`, `"incomplete"`, or `"cancelled"`.
    pub status: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub incomplete_details: Option<IncompleteDetails>,
    pub model: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub previous_response_id: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub instructions: Option<String>,
    pub output: Vec<ResponseItem>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub error: Option<ResponseError>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub tools: Option<Vec<FunctionToolDefinition>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub tool_choice: Option<Value>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub truncation: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub parallel_tool_calls: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub text: Option<TextConfig>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub top_p: Option<f32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub temperature: Option<f32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub presence_penalty: Option<f32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub frequency_penalty: Option<f32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub max_output_tokens: Option<u32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub reasoning: Option<ReasoningConfig>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub store: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub metadata: Option<HashMap<String, String>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub usage: Option<ResponseUsage>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub user: Option<String>,
}

impl ResponseObject {
    /// Concatenates text from the first assistant `message` item in `output`.
    ///
    /// Equivalent to the Python SDK's `response.output_text` property.
    pub fn output_text(&self) -> String {
        for item in &self.output {
            if let ResponseItem::Message { role, content, .. } = item {
                if role == "assistant" {
                    return match content {
                        MessageContent::Text(s) => s.clone(),
                        MessageContent::Parts(parts) => parts
                            .iter()
                            .filter_map(|p| match p {
                                ContentPart::OutputText { text, .. } => Some(text.as_str()),
                                _ => None,
                            })
                            .collect::<Vec<_>>()
                            .join(""),
                    };
                }
            }
        }
        String::new()
    }
}

// ============================================================================
// Streaming Events
// ============================================================================

/// A single Server-Sent Event emitted by the streaming Responses API.
///
/// Discriminated on the `"type"` field.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type")]
pub enum StreamingEvent {
    // ── Response lifecycle ───────────────────────────────────────────────────
    #[serde(rename = "response.created")]
    ResponseCreated {
        response: ResponseObject,
        sequence_number: u64,
    },
    #[serde(rename = "response.queued")]
    ResponseQueued {
        response: ResponseObject,
        sequence_number: u64,
    },
    #[serde(rename = "response.in_progress")]
    ResponseInProgress {
        response: ResponseObject,
        sequence_number: u64,
    },
    #[serde(rename = "response.completed")]
    ResponseCompleted {
        response: ResponseObject,
        sequence_number: u64,
    },
    #[serde(rename = "response.failed")]
    ResponseFailed {
        response: ResponseObject,
        sequence_number: u64,
    },
    #[serde(rename = "response.incomplete")]
    ResponseIncomplete {
        response: ResponseObject,
        sequence_number: u64,
    },

    // ── Output items ─────────────────────────────────────────────────────────
    #[serde(rename = "response.output_item.added")]
    OutputItemAdded {
        item_id: String,
        output_index: u32,
        item: ResponseItem,
        sequence_number: u64,
    },
    #[serde(rename = "response.output_item.done")]
    OutputItemDone {
        item_id: String,
        output_index: u32,
        item: ResponseItem,
        sequence_number: u64,
    },

    // ── Content parts ────────────────────────────────────────────────────────
    #[serde(rename = "response.content_part.added")]
    ContentPartAdded {
        item_id: String,
        output_index: u32,
        content_index: u32,
        part: ContentPart,
        sequence_number: u64,
    },
    #[serde(rename = "response.content_part.done")]
    ContentPartDone {
        item_id: String,
        output_index: u32,
        content_index: u32,
        part: ContentPart,
        sequence_number: u64,
    },

    // ── Text deltas ──────────────────────────────────────────────────────────
    #[serde(rename = "response.output_text.delta")]
    OutputTextDelta {
        item_id: String,
        output_index: u32,
        content_index: u32,
        delta: String,
        #[serde(skip_serializing_if = "Option::is_none")]
        logprobs: Option<Vec<LogProb>>,
        #[serde(skip_serializing_if = "Option::is_none")]
        obfuscation: Option<String>,
        sequence_number: u64,
    },
    #[serde(rename = "response.output_text.done")]
    OutputTextDone {
        item_id: String,
        output_index: u32,
        content_index: u32,
        text: String,
        #[serde(skip_serializing_if = "Option::is_none")]
        logprobs: Option<Vec<LogProb>>,
        sequence_number: u64,
    },
    #[serde(rename = "response.output_text.annotation.added")]
    OutputTextAnnotationAdded {
        item_id: String,
        output_index: u32,
        content_index: u32,
        annotation_index: u32,
        #[serde(skip_serializing_if = "Option::is_none")]
        annotation: Option<Annotation>,
        sequence_number: u64,
    },

    // ── Refusal ──────────────────────────────────────────────────────────────
    #[serde(rename = "response.refusal.delta")]
    RefusalDelta {
        item_id: String,
        output_index: u32,
        content_index: u32,
        delta: String,
        sequence_number: u64,
    },
    #[serde(rename = "response.refusal.done")]
    RefusalDone {
        item_id: String,
        output_index: u32,
        content_index: u32,
        refusal: String,
        sequence_number: u64,
    },

    // ── Function calls ───────────────────────────────────────────────────────
    #[serde(rename = "response.function_call_arguments.delta")]
    FunctionCallArgumentsDelta {
        item_id: String,
        output_index: u32,
        call_id: String,
        delta: String,
        sequence_number: u64,
    },
    #[serde(rename = "response.function_call_arguments.done")]
    FunctionCallArgumentsDone {
        item_id: String,
        output_index: u32,
        call_id: String,
        arguments: String,
        #[serde(skip_serializing_if = "Option::is_none")]
        name: Option<String>,
        sequence_number: u64,
    },

    // ── Reasoning ────────────────────────────────────────────────────────────
    #[serde(rename = "response.reasoning_summary_part.added")]
    ReasoningSummaryPartAdded {
        item_id: String,
        output_index: u32,
        summary_index: u32,
        part: ContentPart,
        sequence_number: u64,
    },
    #[serde(rename = "response.reasoning_summary_part.done")]
    ReasoningSummaryPartDone {
        item_id: String,
        output_index: u32,
        summary_index: u32,
        part: ContentPart,
        sequence_number: u64,
    },
    #[serde(rename = "response.reasoning.delta")]
    ReasoningDelta {
        item_id: String,
        output_index: u32,
        content_index: u32,
        delta: String,
        #[serde(skip_serializing_if = "Option::is_none")]
        obfuscation: Option<String>,
        sequence_number: u64,
    },
    #[serde(rename = "response.reasoning.done")]
    ReasoningDone {
        item_id: String,
        output_index: u32,
        content_index: u32,
        text: String,
        sequence_number: u64,
    },
    #[serde(rename = "response.reasoning_summary_text.delta")]
    ReasoningSummaryTextDelta {
        item_id: String,
        output_index: u32,
        summary_index: u32,
        delta: String,
        #[serde(skip_serializing_if = "Option::is_none")]
        obfuscation: Option<String>,
        sequence_number: u64,
    },
    #[serde(rename = "response.reasoning_summary_text.done")]
    ReasoningSummaryTextDone {
        item_id: String,
        output_index: u32,
        summary_index: u32,
        text: String,
        sequence_number: u64,
    },

    // ── Error ────────────────────────────────────────────────────────────────
    #[serde(rename = "error")]
    Error {
        #[serde(skip_serializing_if = "Option::is_none")]
        code: Option<String>,
        #[serde(skip_serializing_if = "Option::is_none")]
        message: Option<String>,
        #[serde(skip_serializing_if = "Option::is_none")]
        param: Option<String>,
        sequence_number: u64,
    },
}

// ============================================================================
// List / Delete Results
// ============================================================================

/// Result of `DELETE /v1/responses/{id}`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DeleteResponseResult {
    pub id: String,
    pub object: String,
    pub deleted: bool,
}

/// Response from `GET /v1/responses/{id}/input_items`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct InputItemsListResponse {
    pub object: String,
    pub data: Vec<ResponseItem>,
}

/// Response from `GET /v1/responses` (extension endpoint).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ListResponsesResult {
    pub object: String,
    pub data: Vec<ResponseObject>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub first_id: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub last_id: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub has_more: Option<bool>,
}

/// Optional query parameters for `GET /v1/responses`.
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct ListResponsesOptions {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub limit: Option<u32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub order: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub after: Option<String>,
}
