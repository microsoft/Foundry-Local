//! OpenAI-compatible chat completion types.

use serde::{Deserialize, Serialize};

// ─── Request types ───────────────────────────────────────────────────────────

/// A chat completion request message, internally tagged by the `role` field.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[non_exhaustive]
#[serde(tag = "role")]
pub enum ChatCompletionRequestMessage {
    #[serde(rename = "system")]
    System(ChatCompletionRequestSystemMessage),
    #[serde(rename = "user")]
    User(ChatCompletionRequestUserMessage),
    #[serde(rename = "assistant")]
    Assistant(ChatCompletionRequestAssistantMessage),
    #[serde(rename = "tool")]
    Tool(ChatCompletionRequestToolMessage),
    #[serde(rename = "developer")]
    Developer(ChatCompletionRequestDeveloperMessage),
}

/// A system message in a chat completion request.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChatCompletionRequestSystemMessage {
    pub content: String,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub name: Option<String>,
}

impl From<&str> for ChatCompletionRequestSystemMessage {
    fn from(s: &str) -> Self {
        Self::from(s.to_owned())
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

impl From<ChatCompletionRequestSystemMessage> for ChatCompletionRequestMessage {
    fn from(msg: ChatCompletionRequestSystemMessage) -> Self {
        Self::System(msg)
    }
}

/// A user message in a chat completion request.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChatCompletionRequestUserMessage {
    pub content: String,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub name: Option<String>,
}

impl From<&str> for ChatCompletionRequestUserMessage {
    fn from(s: &str) -> Self {
        Self::from(s.to_owned())
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

impl From<ChatCompletionRequestUserMessage> for ChatCompletionRequestMessage {
    fn from(msg: ChatCompletionRequestUserMessage) -> Self {
        Self::User(msg)
    }
}

/// An assistant message in a chat completion request.
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct ChatCompletionRequestAssistantMessage {
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub content: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub name: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub tool_calls: Option<Vec<ChatCompletionMessageToolCalls>>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub refusal: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub function_call: Option<FunctionCall>,
}

impl From<ChatCompletionRequestAssistantMessage> for ChatCompletionRequestMessage {
    fn from(msg: ChatCompletionRequestAssistantMessage) -> Self {
        Self::Assistant(msg)
    }
}

/// A tool result message in a chat completion request.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChatCompletionRequestToolMessage {
    pub content: String,
    pub tool_call_id: String,
}

impl From<ChatCompletionRequestToolMessage> for ChatCompletionRequestMessage {
    fn from(msg: ChatCompletionRequestToolMessage) -> Self {
        Self::Tool(msg)
    }
}

/// A developer message in a chat completion request (replaces system for reasoning models).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChatCompletionRequestDeveloperMessage {
    pub content: String,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub name: Option<String>,
}

impl From<&str> for ChatCompletionRequestDeveloperMessage {
    fn from(s: &str) -> Self {
        Self::from(s.to_owned())
    }
}

impl From<String> for ChatCompletionRequestDeveloperMessage {
    fn from(s: String) -> Self {
        Self {
            content: s,
            name: None,
        }
    }
}

impl From<ChatCompletionRequestDeveloperMessage> for ChatCompletionRequestMessage {
    fn from(msg: ChatCompletionRequestDeveloperMessage) -> Self {
        Self::Developer(msg)
    }
}

/// A tool definition for a chat completion request.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChatCompletionTools {
    #[serde(rename = "type")]
    pub r#type: String,
    pub function: FunctionObject,
}

/// Description of a function that the model can call.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FunctionObject {
    pub name: String,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub description: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub parameters: Option<serde_json::Value>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub strict: Option<bool>,
}

// ─── Response types ──────────────────────────────────────────────────────────

/// Response object for a non-streaming chat completion.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CreateChatCompletionResponse {
    pub id: String,
    pub object: String,
    pub created: u64,
    pub model: String,
    pub choices: Vec<ChatChoice>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub usage: Option<CompletionUsage>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub system_fingerprint: Option<String>,
}

/// A single choice within a non-streaming chat completion response.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChatChoice {
    pub index: u32,
    pub message: ChatCompletionResponseMessage,
    #[serde(default)]
    pub finish_reason: Option<FinishReason>,
}

/// The assistant's message inside a [`ChatChoice`].
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChatCompletionResponseMessage {
    #[serde(default)]
    pub role: Option<String>,
    #[serde(default)]
    pub content: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub tool_calls: Option<Vec<ChatCompletionMessageToolCalls>>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub function_call: Option<FunctionCall>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub refusal: Option<String>,
}

/// Token usage statistics.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CompletionUsage {
    pub prompt_tokens: u32,
    pub completion_tokens: u32,
    pub total_tokens: u32,
}

/// Reason the model stopped generating tokens.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[non_exhaustive]
#[serde(rename_all = "snake_case")]
pub enum FinishReason {
    Stop,
    Length,
    ToolCalls,
    ContentFilter,
    FunctionCall,
}

/// A tool call within a response message, tagged by `type`.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[non_exhaustive]
#[serde(tag = "type")]
pub enum ChatCompletionMessageToolCalls {
    #[serde(rename = "function")]
    Function(ChatCompletionMessageToolCall),
}

/// A single function tool call (id + function payload).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChatCompletionMessageToolCall {
    pub id: String,
    pub function: FunctionCall,
}

/// A resolved function call with name and JSON-encoded arguments.
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct FunctionCall {
    pub name: String,
    pub arguments: String,
}

// ─── Streaming response types ────────────────────────────────────────────────

/// Response object for a streaming chat completion chunk.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CreateChatCompletionStreamResponse {
    pub id: String,
    pub object: String,
    pub created: u64,
    pub model: String,
    pub choices: Vec<ChatChoiceStream>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub usage: Option<CompletionUsage>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub system_fingerprint: Option<String>,
}

/// A single choice within a streaming response chunk.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChatChoiceStream {
    pub index: u32,
    pub delta: ChatCompletionStreamResponseDelta,
    #[serde(default)]
    pub finish_reason: Option<FinishReason>,
}

/// The delta payload inside a streaming [`ChatChoiceStream`].
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChatCompletionStreamResponseDelta {
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub role: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub content: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub tool_calls: Option<Vec<ChatCompletionMessageToolCallChunk>>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub function_call: Option<FunctionCallStream>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub refusal: Option<String>,
}

/// A partial tool call chunk received during streaming.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChatCompletionMessageToolCallChunk {
    pub index: u32,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub id: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none", rename = "type")]
    pub r#type: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub function: Option<FunctionCallStream>,
}

/// A partial function call received during streaming.
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct FunctionCallStream {
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub name: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub arguments: Option<String>,
}
