//! Custom OpenAI-compatible chat completion types.
//!
//! These replace the `async-openai` dependency with lightweight, serde-driven DTOs
//! that cover the subset of the OpenAI chat completions API used by Foundry Local.

use serde::{Deserialize, Serialize};

// ---------------------------------------------------------------------------
// Request message types
// ---------------------------------------------------------------------------

/// Union type for all chat message variants.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "role", rename_all = "snake_case")]
pub enum ChatCompletionRequestMessage {
    System(ChatCompletionRequestSystemMessage),
    User(ChatCompletionRequestUserMessage),
    Assistant(ChatCompletionRequestAssistantMessage),
    Tool(ChatCompletionRequestToolMessage),
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChatCompletionRequestSystemMessage {
    pub content: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub name: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChatCompletionRequestUserMessage {
    pub content: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub name: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChatCompletionRequestAssistantMessage {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub content: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub name: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub tool_calls: Option<Vec<ChatCompletionMessageToolCall>>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChatCompletionRequestToolMessage {
    pub content: String,
    pub tool_call_id: String,
}

// -- From impls for ergonomic message construction -------------------------

impl From<&str> for ChatCompletionRequestSystemMessage {
    fn from(s: &str) -> Self {
        Self {
            content: s.to_owned(),
            name: None,
        }
    }
}

impl From<String> for ChatCompletionRequestSystemMessage {
    fn from(s: String) -> Self {
        Self {
            content: s,
            name: None,
        }
    }
}

impl From<&str> for ChatCompletionRequestUserMessage {
    fn from(s: &str) -> Self {
        Self {
            content: s.to_owned(),
            name: None,
        }
    }
}

impl From<String> for ChatCompletionRequestUserMessage {
    fn from(s: String) -> Self {
        Self {
            content: s,
            name: None,
        }
    }
}

impl From<ChatCompletionRequestSystemMessage> for ChatCompletionRequestMessage {
    fn from(m: ChatCompletionRequestSystemMessage) -> Self {
        Self::System(m)
    }
}

impl From<ChatCompletionRequestUserMessage> for ChatCompletionRequestMessage {
    fn from(m: ChatCompletionRequestUserMessage) -> Self {
        Self::User(m)
    }
}

impl From<ChatCompletionRequestAssistantMessage> for ChatCompletionRequestMessage {
    fn from(m: ChatCompletionRequestAssistantMessage) -> Self {
        Self::Assistant(m)
    }
}

impl From<ChatCompletionRequestToolMessage> for ChatCompletionRequestMessage {
    fn from(m: ChatCompletionRequestToolMessage) -> Self {
        Self::Tool(m)
    }
}

// ---------------------------------------------------------------------------
// Tool definition types
// ---------------------------------------------------------------------------

/// Tool definition for function calling.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChatCompletionTools {
    pub r#type: String,
    pub function: FunctionObject,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FunctionObject {
    pub name: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub description: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub parameters: Option<serde_json::Value>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub strict: Option<bool>,
}

// ---------------------------------------------------------------------------
// Tool choice types
// ---------------------------------------------------------------------------

/// Tool choice configuration.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(untagged)]
pub enum ChatCompletionToolChoiceOption {
    Simple(String),
    Named(ChatCompletionNamedToolChoice),
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChatCompletionNamedToolChoice {
    pub r#type: String,
    pub function: ChatCompletionNamedToolChoiceFunction,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChatCompletionNamedToolChoiceFunction {
    pub name: String,
}

// ---------------------------------------------------------------------------
// Non-streaming response types
// ---------------------------------------------------------------------------

/// Non-streaming chat completion response.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CreateChatCompletionResponse {
    pub id: String,
    pub object: String,
    pub created: u64,
    pub model: String,
    pub choices: Vec<ChatChoice>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub usage: Option<CompletionUsage>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub system_fingerprint: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChatChoice {
    pub index: u32,
    pub message: ChatCompletionResponseMessage,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub finish_reason: Option<FinishReason>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub logprobs: Option<serde_json::Value>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChatCompletionResponseMessage {
    pub role: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub content: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub tool_calls: Option<Vec<ChatCompletionMessageToolCall>>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CompletionUsage {
    pub prompt_tokens: u32,
    pub completion_tokens: u32,
    pub total_tokens: u32,
}

// ---------------------------------------------------------------------------
// Streaming response types
// ---------------------------------------------------------------------------

/// Streaming chat completion chunk.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CreateChatCompletionStreamResponse {
    pub id: String,
    pub object: String,
    pub created: u64,
    pub model: String,
    pub choices: Vec<ChatChoiceStream>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub usage: Option<CompletionUsage>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub system_fingerprint: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChatChoiceStream {
    pub index: u32,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub delta: Option<ChatCompletionStreamResponseDelta>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub finish_reason: Option<FinishReason>,
    // Foundry core sometimes returns "message" instead of "delta" in streaming.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub message: Option<ChatCompletionResponseMessage>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChatCompletionStreamResponseDelta {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub role: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub content: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub tool_calls: Option<Vec<ChatCompletionMessageToolCallChunk>>,
}

// ---------------------------------------------------------------------------
// Tool call types (shared between request and response)
// ---------------------------------------------------------------------------

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChatCompletionMessageToolCall {
    pub id: String,
    pub r#type: String,
    pub function: FunctionCall,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChatCompletionMessageToolCallChunk {
    pub index: u32,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub id: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub r#type: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub function: Option<FunctionCallStream>,
}

/// Discriminated union for tool call types.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum ChatCompletionMessageToolCalls {
    Function(FunctionCall),
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FunctionCall {
    pub name: String,
    pub arguments: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FunctionCallStream {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub name: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub arguments: Option<String>,
}

// ---------------------------------------------------------------------------
// FinishReason
// ---------------------------------------------------------------------------

/// Reason the model stopped generating.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum FinishReason {
    Stop,
    Length,
    ToolCalls,
    ContentFilter,
    FunctionCall,
}
