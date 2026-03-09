//! Foundry Local Rust SDK
//!
//! Local AI model inference powered by the Foundry Local Core engine.

mod error;
mod types;
mod configuration;
mod foundry_local_manager;
mod catalog;
mod model;
mod model_variant;

pub(crate) mod detail;
pub mod openai;

pub use error::FoundryLocalError;
pub use types::*;
pub use configuration::{FoundryLocalConfig, LogLevel};
pub use foundry_local_manager::FoundryLocalManager;
pub use catalog::Catalog;
pub use model::Model;
pub use model_variant::ModelVariant;
pub use detail::ModelLoadManager;

// Re-export OpenAI request types so callers can construct typed messages.
pub use async_openai::types::chat::{
    ChatCompletionRequestMessage,
    ChatCompletionRequestSystemMessage,
    ChatCompletionRequestUserMessage,
    ChatCompletionRequestAssistantMessage,
    ChatCompletionRequestToolMessage,
    ChatCompletionTools,
    ChatCompletionToolChoiceOption,
    ChatCompletionNamedToolChoice,
    FunctionObject,
};

// Re-export OpenAI response types for convenience.
pub use async_openai::types::chat::{
    CreateChatCompletionResponse,
    CreateChatCompletionStreamResponse,
    ChatChoice,
    ChatChoiceStream,
    ChatCompletionResponseMessage,
    ChatCompletionStreamResponseDelta,
    ChatCompletionMessageToolCall,
    ChatCompletionMessageToolCalls,
    ChatCompletionMessageToolCallChunk,
    FunctionCall,
    FunctionCallStream,
    FinishReason,
    CompletionUsage,
};
pub use crate::openai::{
    AudioTranscriptionResponse, ChatCompletionStream, AudioTranscriptionStream,
};
