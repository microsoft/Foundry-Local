//! Foundry Local Rust SDK
//!
//! Local AI model inference powered by the Foundry Local Core engine.

mod catalog;
mod configuration;
mod error;
mod foundry_local_manager;
mod types;

pub(crate) mod detail;
pub mod openai;

pub use self::catalog::Catalog;
pub use self::configuration::{FoundryLocalConfig, LogLevel, Logger};
pub use self::detail::model::Model;
pub use self::error::FoundryLocalError;
pub use self::foundry_local_manager::FoundryLocalManager;
pub use self::types::{
    ChatResponseFormat, ChatToolChoice, DeviceType, EpDownloadResult, EpInfo, ModelInfo,
    ModelSettings, Parameter, PromptTemplate, Runtime,
};

// Re-export OpenAI-compatible chat completion types.
pub use crate::openai::chat_types::{
    ChatChoice, ChatChoiceStream, ChatCompletionMessageToolCall,
    ChatCompletionMessageToolCallChunk, ChatCompletionMessageToolCalls,
    ChatCompletionNamedToolChoice, ChatCompletionRequestAssistantMessage,
    ChatCompletionRequestMessage, ChatCompletionRequestSystemMessage,
    ChatCompletionRequestToolMessage, ChatCompletionRequestUserMessage,
    ChatCompletionResponseMessage, ChatCompletionStreamResponseDelta,
    ChatCompletionToolChoiceOption, ChatCompletionTools, CompletionUsage,
    CreateChatCompletionResponse, CreateChatCompletionStreamResponse, FinishReason, FunctionCall,
    FunctionCallStream, FunctionObject,
};

// Re-export audio and streaming types for convenience.
pub use crate::openai::{
    AudioTranscriptionResponse, AudioTranscriptionStream, ChatCompletionStream,
    TranscriptionSegment, TranscriptionWord,
};
