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

pub use self::catalog::Catalog;
pub use self::configuration::{FoundryLocalConfig, LogLevel, Logger};
pub use self::error::FoundryLocalError;
pub use self::foundry_local_manager::FoundryLocalManager;
pub use self::model::Model;
pub use self::model_variant::ModelVariant;
pub use self::types::{
    ChatResponseFormat, ChatToolChoice, DeviceType, ModelInfo, ModelSettings, Parameter,
    PromptTemplate, Runtime,
};

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
    TranscriptionSegment, TranscriptionWord,
};
pub use async_openai::types::chat::{
    ChatChoice, ChatChoiceStream, ChatCompletionMessageToolCall,
    ChatCompletionMessageToolCallChunk, ChatCompletionMessageToolCalls,
    ChatCompletionResponseMessage, ChatCompletionStreamResponseDelta, CompletionUsage,
    CreateChatCompletionResponse, CreateChatCompletionStreamResponse, FinishReason, FunctionCall,
    FunctionCallStream,
};
