# Foundry Local Rust SDK — Public API Reference

> Auto-generated from `sdk_v2/rust/src` source files.

## Table of Contents

- [Entry Point](#entry-point)
  - [FoundryLocalManager](#foundrylocalmanager)
  - [FoundryLocalConfig](#foundrylocalconfig)
  - [LogLevel](#loglevel)
- [Model Catalog](#model-catalog)
  - [Catalog](#catalog)
  - [Model](#model)
  - [ModelVariant](#modelvariant)
- [OpenAI Clients](#openai-clients)
  - [ChatClient](#chatclient)
  - [ChatCompletionStream](#chatcompletionstream)
  - [AudioClient](#audioclient)
  - [AudioTranscriptionStream](#audiotranscriptionstream)
  - [AudioTranscriptionResponse](#audiotranscriptionresponse)
  - [JsonStream\<T\>](#jsonstreamt)
- [Types](#types)
  - [ModelInfo](#modelinfo)
  - [ChatResponseFormat](#chatresponseformat)
  - [ChatToolChoice](#chattoolchoice)
  - [DeviceType](#devicetype)
  - [PromptTemplate](#prompttemplate)
  - [Runtime](#runtime)
  - [ModelSettings](#modelsettings)
  - [Parameter](#parameter)
- [Error Handling](#error-handling)
  - [FoundryLocalError](#foundrylocalerror)
  - [Result\<T\>](#resultt)
- [Re-exported OpenAI Types](#re-exported-openai-types)

---

## Entry Point

### FoundryLocalManager

Primary entry point for interacting with Foundry Local. Singleton — created once via `create()`.

```rust
pub struct FoundryLocalManager { /* private fields */ }
```

| Method | Signature | Description |
|--------|-----------|-------------|
| `create` | `fn create(config: FoundryLocalConfig) -> Result<&'static Self>` | Initialise the SDK. First call creates the singleton; subsequent calls return the existing instance (config is ignored after first call). |
| `catalog` | `fn catalog(&self) -> &Catalog` | Access the model catalog. |
| `urls` | `fn urls(&self) -> Result<Vec<String>>` | URLs the local web service is listening on. Empty until `start_web_service` is called. |
| `start_web_service` | `async fn start_web_service(&self) -> Result<Vec<String>>` | Start the local web service and return listening URLs. |
| `stop_web_service` | `async fn stop_web_service(&self) -> Result<()>` | Stop the local web service. |

---

### FoundryLocalConfig

User-facing configuration for initializing the SDK.

```rust
#[non_exhaustive]
pub struct FoundryLocalConfig {
    pub app_name: String,
    pub app_data_dir: Option<String>,
    pub model_cache_dir: Option<String>,
    pub logs_dir: Option<String>,
    pub log_level: Option<LogLevel>,
    pub web_service_urls: Option<String>,
    pub service_endpoint: Option<String>,
    pub library_path: Option<String>,
    pub additional_settings: Option<HashMap<String, String>>,
}
```

| Method | Signature | Description |
|--------|-----------|-------------|
| `new` | `fn new(app_name: impl Into<String>) -> Self` | Create a new configuration. All fields default to `None`. |

**Example:**
```rust
let config = FoundryLocalConfig {
    log_level: Some(LogLevel::Debug),
    ..FoundryLocalConfig::new("my_app")
};
```

---

### LogLevel

```rust
pub enum LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Fatal,
}
```

---

## Model Catalog

### Catalog

Discovers, caches, and looks up available models.

```rust
pub struct Catalog { /* private fields */ }
```

| Method | Signature | Description |
|--------|-----------|-------------|
| `name` | `fn name(&self) -> &str` | Catalog name as reported by the native core. |
| `update_models` | `async fn update_models(&self) -> Result<()>` | Refresh catalog if cache expired or invalidated. |
| `get_models` | `async fn get_models(&self) -> Result<Vec<Arc<Model>>>` | Return all known models. |
| `get_model` | `async fn get_model(&self, alias: &str) -> Result<Arc<Model>>` | Look up a model by alias. |
| `get_model_variant` | `async fn get_model_variant(&self, id: &str) -> Result<Arc<ModelVariant>>` | Look up a variant by unique id. |
| `get_cached_models` | `async fn get_cached_models(&self) -> Result<Vec<Arc<ModelVariant>>>` | Return only variants cached on disk. |
| `get_loaded_models` | `async fn get_loaded_models(&self) -> Result<Vec<String>>` | Return ids of models currently loaded in memory. |

---

### Model

Groups one or more `ModelVariant`s sharing the same alias. By default, the cached variant is selected.

```rust
pub struct Model { /* private fields */ }
```

| Method | Signature | Description |
|--------|-----------|-------------|
| `alias` | `fn alias(&self) -> &str` | Alias shared by all variants. |
| `id` | `fn id(&self) -> &str` | Unique identifier of the selected variant. |
| `variants` | `fn variants(&self) -> &[ModelVariant]` | All variants in this model. |
| `selected_variant` | `fn selected_variant(&self) -> &ModelVariant` | Currently selected variant. |
| `select_variant` | `fn select_variant(&mut self, id: &str) -> Result<()>` | Select a variant by id. |
| `is_cached` | `async fn is_cached(&self) -> Result<bool>` | Whether the selected variant is cached on disk. |
| `is_loaded` | `async fn is_loaded(&self) -> Result<bool>` | Whether the selected variant is loaded in memory. |
| `download` | `async fn download<F>(&self, progress: Option<F>) -> Result<()>` | Download the selected variant. `F: FnMut(&str) + Send + 'static` |
| `path` | `async fn path(&self) -> Result<PathBuf>` | Local file-system path of the selected variant. |
| `load` | `async fn load(&self) -> Result<()>` | Load the selected variant into memory. |
| `unload` | `async fn unload(&self) -> Result<String>` | Unload the selected variant from memory. |
| `remove_from_cache` | `async fn remove_from_cache(&self) -> Result<String>` | Remove the selected variant from the local cache. |
| `create_chat_client` | `fn create_chat_client(&self) -> ChatClient` | Create a ChatClient bound to the selected variant. |
| `create_audio_client` | `fn create_audio_client(&self) -> AudioClient` | Create an AudioClient bound to the selected variant. |

---

### ModelVariant

A single model variant — one specific id within an alias group.

```rust
pub struct ModelVariant { /* private fields */ }
```

| Method | Signature | Description |
|--------|-----------|-------------|
| `info` | `fn info(&self) -> &ModelInfo` | Full metadata for this variant. |
| `id` | `fn id(&self) -> &str` | Unique identifier. |
| `alias` | `fn alias(&self) -> &str` | Alias shared with sibling variants. |
| `is_cached` | `async fn is_cached(&self) -> Result<bool>` | Whether cached locally. ⚠️ Full IPC per call — prefer `Catalog::get_cached_models()` for batch use. |
| `is_loaded` | `async fn is_loaded(&self) -> Result<bool>` | Whether currently loaded in memory. |
| `download` | `async fn download<F>(&self, progress: Option<F>) -> Result<()>` | Download the variant. `F: FnMut(&str) + Send + 'static` |
| `path` | `async fn path(&self) -> Result<PathBuf>` | Local file-system path. |
| `load` | `async fn load(&self) -> Result<()>` | Load into memory. |
| `unload` | `async fn unload(&self) -> Result<String>` | Unload from memory. |
| `remove_from_cache` | `async fn remove_from_cache(&self) -> Result<String>` | Remove from local cache. |
| `create_chat_client` | `fn create_chat_client(&self) -> ChatClient` | Create a ChatClient bound to this variant. |
| `create_audio_client` | `fn create_audio_client(&self) -> AudioClient` | Create an AudioClient bound to this variant. |

---

## OpenAI Clients

### ChatClient

OpenAI-compatible chat completions backed by a local model. Uses a consuming builder pattern.

```rust
pub struct ChatClient { /* private fields */ }
```

**Builder methods** (all `mut self -> Self`):

| Method | Signature | Description |
|--------|-----------|-------------|
| `frequency_penalty` | `fn frequency_penalty(mut self, v: f64) -> Self` | Set the frequency penalty. |
| `max_tokens` | `fn max_tokens(mut self, v: u32) -> Self` | Maximum tokens to generate. |
| `n` | `fn n(mut self, v: u32) -> Self` | Number of completions. |
| `temperature` | `fn temperature(mut self, v: f64) -> Self` | Sampling temperature. |
| `presence_penalty` | `fn presence_penalty(mut self, v: f64) -> Self` | Presence penalty. |
| `top_p` | `fn top_p(mut self, v: f64) -> Self` | Nucleus sampling probability. |
| `top_k` | `fn top_k(mut self, v: u32) -> Self` | Top-k sampling *(Foundry extension)*. |
| `random_seed` | `fn random_seed(mut self, v: u64) -> Self` | Random seed for reproducibility *(Foundry extension)*. |
| `response_format` | `fn response_format(mut self, v: ChatResponseFormat) -> Self` | Desired response format. |
| `tool_choice` | `fn tool_choice(mut self, v: ChatToolChoice) -> Self` | Tool choice strategy. |

**Completion methods:**

| Method | Signature | Description |
|--------|-----------|-------------|
| `complete_chat` | `async fn complete_chat(&self, messages: &[ChatCompletionRequestMessage], tools: Option<&[ChatCompletionTools]>) -> Result<CreateChatCompletionResponse>` | Non-streaming chat completion. |
| `complete_streaming_chat` | `async fn complete_streaming_chat(&self, messages: &[ChatCompletionRequestMessage], tools: Option<&[ChatCompletionTools]>) -> Result<ChatCompletionStream>` | Streaming chat completion. |

**Example:**
```rust
let client = model.create_chat_client()
    .temperature(0.7)
    .max_tokens(256);
```

---

### ChatCompletionStream

```rust
pub type ChatCompletionStream = JsonStream<CreateChatCompletionStreamResponse>;
```

A stream of `CreateChatCompletionStreamResponse` chunks. Use with `StreamExt::next()`.

---

### AudioClient

OpenAI-compatible audio transcription backed by a local model.

```rust
pub struct AudioClient { /* private fields */ }
```

**Builder methods** (all `mut self -> Self`):

| Method | Signature | Description |
|--------|-----------|-------------|
| `language` | `fn language(mut self, lang: impl Into<String>) -> Self` | Language hint for transcription. |
| `temperature` | `fn temperature(mut self, v: f64) -> Self` | Sampling temperature. |

**Transcription methods:**

| Method | Signature | Description |
|--------|-----------|-------------|
| `transcribe` | `async fn transcribe(&self, audio_file_path: impl AsRef<Path>) -> Result<AudioTranscriptionResponse>` | Transcribe an audio file. |
| `transcribe_streaming` | `async fn transcribe_streaming(&self, audio_file_path: impl AsRef<Path>) -> Result<AudioTranscriptionStream>` | Streaming transcription. |

**Example:**
```rust
let client = model.create_audio_client()
    .language("en")
    .temperature(0.2);
```

---

### AudioTranscriptionStream

```rust
pub type AudioTranscriptionStream = JsonStream<AudioTranscriptionResponse>;
```

A stream of `AudioTranscriptionResponse` chunks. Use with `StreamExt::next()`.

---

### AudioTranscriptionResponse

```rust
pub struct AudioTranscriptionResponse {
    pub text: String,                              // The transcribed text
    pub language: Option<String>,                  // Language of input audio (if detected)
    pub duration: Option<f64>,                     // Duration in seconds (if available)
    pub segments: Option<Vec<serde_json::Value>>,  // Transcription segments (if available)
    pub words: Option<Vec<serde_json::Value>>,     // Words with timestamps (if available)
}
```

Derives: `Debug`, `Clone`, `Deserialize`, `Serialize`

---

### JsonStream\<T\>

Generic stream that deserializes each received JSON string chunk into `T`. Empty chunks are silently skipped.

```rust
pub struct JsonStream<T> { /* private fields */ }

impl<T> Unpin for JsonStream<T> {}
impl<T: DeserializeOwned> Stream for JsonStream<T> {
    type Item = Result<T>;
}
```

---

## Types

### ModelInfo

Full metadata for a model variant as returned by the catalog.

```rust
pub struct ModelInfo {
    pub id: String,
    pub name: String,
    pub version: i64,
    pub alias: String,
    pub display_name: Option<String>,
    pub provider_type: String,
    pub uri: String,
    pub model_type: String,
    pub prompt_template: Option<PromptTemplate>,
    pub publisher: Option<String>,
    pub model_settings: Option<ModelSettings>,
    pub license: Option<String>,
    pub license_description: Option<String>,
    pub cached: bool,
    pub task: Option<String>,
    pub runtime: Option<Runtime>,
    pub file_size_mb: Option<f64>,
    pub supports_tool_calling: Option<bool>,
    pub max_output_tokens: Option<i64>,
    pub min_fl_version: Option<String>,
    pub created_at_unix: i64,
}
```

Derives: `Debug`, `Clone`, `Deserialize`

---

### ChatResponseFormat

```rust
pub enum ChatResponseFormat {
    Text,                   // Plain text output (default)
    JsonObject,             // JSON output (unstructured)
    JsonSchema(String),     // JSON constrained by schema string
    LarkGrammar(String),    // Lark grammar constraint (Foundry extension)
}
```

---

### ChatToolChoice

```rust
pub enum ChatToolChoice {
    None,               // Model will not call any tool
    Auto,               // Model decides whether to call a tool
    Required,           // Model must call at least one tool
    Function(String),   // Model must call the named function
}
```

---

### DeviceType

```rust
pub enum DeviceType {
    Invalid,
    CPU,
    GPU,
    NPU,
}
```

---

### PromptTemplate

```rust
pub struct PromptTemplate {
    pub system: Option<String>,
    pub user: Option<String>,
    pub assistant: Option<String>,
    pub prompt: Option<String>,
}
```

---

### Runtime

```rust
pub struct Runtime {
    pub device_type: DeviceType,
    pub execution_provider: String,
}
```

---

### ModelSettings

```rust
pub struct ModelSettings {
    pub parameters: Option<Vec<Parameter>>,
}
```

---

### Parameter

```rust
pub struct Parameter {
    pub name: String,
    pub value: Option<String>,
}
```

---

## Error Handling

### FoundryLocalError

```rust
pub enum FoundryLocalError {
    /// The native core library could not be loaded.
    LibraryLoad { reason: String },

    /// A command executed against the native core returned an error.
    CommandExecution { reason: String },

    /// The provided configuration is invalid.
    InvalidConfiguration { reason: String },

    /// A model operation failed (load, unload, download, etc.).
    ModelOperation { reason: String },

    /// An HTTP request to the external service failed.
    HttpRequest(reqwest::Error),

    /// Serialization or deserialization of JSON data failed.
    Serialization(serde_json::Error),

    /// A validation check on user-supplied input failed.
    Validation { reason: String },

    /// An I/O error occurred.
    Io(std::io::Error),

    /// An internal SDK error (e.g. poisoned lock).
    Internal { reason: String },
}
```

Implements: `Display`, `Error`, `From<serde_json::Error>`, `From<std::io::Error>`, `From<reqwest::Error>`

---

### Result\<T\>

```rust
pub type Result<T> = std::result::Result<T, FoundryLocalError>;
```

Convenience alias used throughout the SDK.

---

## Re-exported OpenAI Types

The following types from `async_openai` are re-exported at the crate root for convenience:

**Request types:**
- `ChatCompletionRequestMessage`
- `ChatCompletionRequestSystemMessage`
- `ChatCompletionRequestUserMessage`
- `ChatCompletionRequestAssistantMessage`
- `ChatCompletionRequestToolMessage`
- `ChatCompletionTools`
- `ChatCompletionToolChoiceOption`
- `ChatCompletionNamedToolChoice`
- `FunctionObject`

**Response types:**
- `CreateChatCompletionResponse`
- `CreateChatCompletionStreamResponse`
- `ChatChoice`
- `ChatChoiceStream`
- `ChatCompletionResponseMessage`
- `ChatCompletionStreamResponseDelta`
- `CompletionUsage`
- `FinishReason`

**Tool call types:**
- `ChatCompletionMessageToolCall`
- `ChatCompletionMessageToolCallChunk`
- `ChatCompletionMessageToolCalls`
- `FunctionCall`
- `FunctionCallStream`
