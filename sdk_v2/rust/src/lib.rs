//! Foundry Local Rust SDK
//!
//! Local AI model inference powered by the Foundry Local Core engine.

mod catalog;
mod configuration;
mod error;
mod foundry_local_manager;
mod model;
mod model_variant;
mod types;

pub(crate) mod detail;
pub mod openai;

pub use catalog::Catalog;
pub use configuration::{FoundryLocalConfig, LogLevel};
pub use detail::ModelLoadManager;
pub use error::FoundryLocalError;
pub use foundry_local_manager::FoundryLocalManager;
pub use model::Model;
pub use model_variant::ModelVariant;
pub use types::*;

// Re-export OpenAI request types so callers can construct typed messages.
pub use async_openai::types::chat::{
    ChatCompletionNamedToolChoice, ChatCompletionRequestAssistantMessage,
    ChatCompletionRequestMessage, ChatCompletionRequestSystemMessage,
    ChatCompletionRequestToolMessage, ChatCompletionRequestUserMessage,
    ChatCompletionToolChoiceOption, ChatCompletionTools, FunctionObject,
};

// Re-export OpenAI response types for convenience.
pub use crate::openai::{
    AudioTranscriptionResponse, AudioTranscriptionStream, ChatCompletionStream,
};
pub use async_openai::types::chat::{
    ChatChoice, ChatChoiceStream, ChatCompletionMessageToolCall,
    ChatCompletionMessageToolCallChunk, ChatCompletionMessageToolCalls,
    ChatCompletionResponseMessage, ChatCompletionStreamResponseDelta, CompletionUsage,
    CreateChatCompletionResponse, CreateChatCompletionStreamResponse, FinishReason, FunctionCall,
    FunctionCallStream,
};
