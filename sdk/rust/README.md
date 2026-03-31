# Foundry Local Rust SDK

The Foundry Local Rust SDK provides an async Rust interface for running AI models locally on your machine. Discover, download, load, and run inference — all without cloud dependencies.

## Features

- **Local-first AI** — Run models entirely on your machine with no cloud calls
- **Model catalog** — Browse and discover available models; check what's cached or loaded
- **Automatic model management** — Download, load, unload, and remove models from cache
- **Chat completions** — OpenAI-compatible chat API with both non-streaming and streaming responses
- **Audio transcription** — Transcribe audio files locally with streaming support
- **Tool calling** — Function/tool calling with streaming, multi-turn conversation support
- **Response format control** — Text, JSON, JSON Schema, and Lark grammar constrained output
- **Multi-variant models** — Models can have multiple variants (e.g., different quantizations) with automatic selection of the best cached variant
- **Embedded web service** — Start a local HTTP server for OpenAI-compatible API access
- **WinML support** — Automatic execution provider download on Windows for NPU/GPU acceleration
- **EP discovery and progress** — Discover available EPs, download selectively with per-EP progress callbacks
- **Configurable inference** — Control temperature, max tokens, top-k, top-p, frequency penalty, random seed, and more
- **Async-first** — Every operation is `async`; designed for use with the `tokio` runtime
- **Safe FFI** — Dynamically loads the native Foundry Local Core engine with a safe Rust wrapper

## Prerequisites

- **Rust** 1.70+ (stable toolchain)
- An internet connection during first build (to download native libraries)

## Installation

```sh
cargo add foundry-local-sdk
```

Or add to your `Cargo.toml`:

```toml
[dependencies]
foundry-local-sdk = "0.1"
```

You also need an async runtime. Most examples use [tokio](https://crates.io/crates/tokio):

```toml
[dependencies]
tokio = { version = "1", features = ["rt-multi-thread", "macros"] }
tokio-stream = "0.1"       # for StreamExt on streaming responses
```

### Feature Flags

| Feature   | Description |
|-----------|-------------|
| `winml`   | Use the WinML backend (Windows only). Selects different ONNX Runtime and GenAI packages for NPU/GPU acceleration. |
| `nightly` | Resolve the latest nightly build of the Core package from the ORT-Nightly feed. |

Enable features in `Cargo.toml`:

```toml
[dependencies]
foundry-local-sdk = { version = "0.1", features = ["winml"] }
```

> **Note:** The `winml` feature is only relevant on Windows. On macOS and Linux, the standard build is used regardless. No code changes are needed — your application code stays the same.

### EP discovery and per-EP progress

You can discover which EPs are available and monitor individual download progress:

```rust
// Discover available execution providers
let eps = manager.discover_eps()?;
for ep in &eps {
    println!("  {} (registered: {})", ep.name, ep.is_registered);
}

// Download with per-EP progress reporting
let ep_names: Vec<&str> = eps.iter().map(|ep| ep.name.as_str()).collect();
manager.ensure_eps_downloaded(
    Some(&ep_names),
    Some(|name: &str, percent: f64| {
        print!("\r  {name}: {percent:.1}%");
        std::io::Write::flush(&mut std::io::stdout()).ok();
    }),
).await?;
```

`discover_eps()` returns a `Vec<EpInfo>` with each EP's `name` and `is_registered` status. The progress callback receives the EP name and a percentage (0–100) as each EP downloads.

## Quick Start

```rust
use foundry_local_sdk::{
    ChatCompletionRequestMessage, ChatCompletionRequestSystemMessage,
    ChatCompletionRequestUserMessage, FoundryLocalConfig, FoundryLocalManager,
};

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    // 1. Initialize the manager — loads native libraries and starts the engine
    let manager = FoundryLocalManager::create(FoundryLocalConfig::new("my_app"))?;

    // 2. Get a model from the catalog and load it
    let model = manager.catalog().get_model("phi-3.5-mini").await?;
    model.load().await?;

    // 3. Create a chat client and run inference
    let client = model.create_chat_client()
        .temperature(0.7)
        .max_tokens(256);

    let messages: Vec<ChatCompletionRequestMessage> = vec![
        ChatCompletionRequestSystemMessage::from("You are a helpful assistant.").into(),
        ChatCompletionRequestUserMessage::from("What is the capital of France?").into(),
    ];

    let response = client.complete_chat(&messages, None).await?;
    println!("{}", response.choices[0].message.content.as_deref().unwrap_or(""));

    // 4. Clean up
    model.unload().await?;

    Ok(())
}
```

## Usage

### Browsing the Model Catalog

The `Catalog` lets you discover what models are available, which are already cached locally, and which are currently loaded in memory.

```rust
let catalog = manager.catalog();

// List all available models
let models = catalog.get_models().await?;
for model in &models {
    println!("{} (id: {})", model.alias(), model.id());
}

// Look up a specific model by alias
let model = catalog.get_model("phi-3.5-mini").await?;

// Look up a specific variant by its unique model ID
let variant = catalog.get_model_variant("phi-3.5-mini-generic-gpu-4").await?;

// See what's already downloaded
let cached = catalog.get_cached_models().await?;

// See what's currently loaded in memory
let loaded = catalog.get_loaded_models().await?;
```

### Model Lifecycle

Each `Model` wraps one or more `ModelVariant` entries (different quantizations, hardware targets). The SDK auto-selects the best available variant, preferring cached versions.

```rust
let model = catalog.get_model("phi-3.5-mini").await?;

// Inspect available variants
println!("Selected: {}", model.selected_variant().id());
for v in model.variants() {
    println!("  {} (cached: {})", v.id(), v.info().cached);
}
```

Download, load, and unload:

```rust
// Download with progress reporting
model.download(Some(|progress: &str| {
    print!("\r{progress}");
    std::io::Write::flush(&mut std::io::stdout()).ok();
})).await?;

// Load into memory
model.load().await?;

// Unload when done
model.unload().await?;

// Remove from local cache entirely
model.remove_from_cache().await?;
```

### Chat Completions

The `ChatClient` follows the OpenAI Chat Completion API structure.

```rust
let client = model.create_chat_client()

// Configure generation settings (fluent builder)
    .temperature(0.7)
    .max_tokens(256)
    .top_p(0.9)
    .frequency_penalty(0.5);

// Non-streaming completion
let response = client.complete_chat(
    &[
        ChatCompletionRequestSystemMessage::from("You are a helpful assistant.").into(),
        ChatCompletionRequestUserMessage::from("Explain Rust's ownership model.").into(),
    ],
    None,
).await?;

println!("{}", response.choices[0].message.content.as_deref().unwrap_or(""));
```

### Streaming Responses

For real-time token-by-token output, use streaming:

```rust
use tokio_stream::StreamExt;

let mut stream = client.complete_streaming_chat(
    &[ChatCompletionRequestUserMessage::from("Write a short poem about Rust.").into()],
    None,
).await?;

while let Some(chunk) = stream.next().await {
    let chunk = chunk?;
    if let Some(content) = &chunk.choices[0].delta.content {
        print!("{content}");
    }
}

// Errors from the native core are delivered as stream items —
// no separate close() call needed.
```

### Tool Calling

Define functions the model can call and handle the multi-turn conversation:

```rust
use foundry_local_sdk::{
    ChatCompletionRequestMessage, ChatCompletionRequestToolMessage,
    ChatCompletionTools, ChatToolChoice, FinishReason,
};
use serde_json::json;

// Define available tools
let tools: Vec<ChatCompletionTools> = serde_json::from_value(json!([{
    "type": "function",
    "function": {
        "name": "get_weather",
        "description": "Get the current weather for a location",
        "parameters": {
            "type": "object",
            "properties": {
                "location": { "type": "string", "description": "City name" }
            },
            "required": ["location"]
        }
    }
}]))?;

let client = model.create_chat_client()
    .max_tokens(512)
    .tool_choice(ChatToolChoice::Auto);

let mut messages: Vec<ChatCompletionRequestMessage> = vec![
    ChatCompletionRequestUserMessage::from("What's the weather in Seattle?").into(),
];

// First request — model may call a tool
let response = client.complete_chat(&messages, Some(&tools)).await?;
let choice = &response.choices[0];

if choice.finish_reason == Some(FinishReason::ToolCalls) {
    if let Some(tool_calls) = &choice.message.tool_calls {
        for tc in tool_calls {
            // Execute the tool (your application logic)
            let result = execute_tool(&tc.function.name, &tc.function.arguments);

            // Add assistant message with tool calls, then the tool result
            messages.push(serde_json::from_value(json!({
                "role": "assistant",
                "content": null,
                "tool_calls": [{ "id": tc.id, "type": "function",
                    "function": { "name": tc.function.name,
                                  "arguments": tc.function.arguments } }]
            }))?);
            messages.push(ChatCompletionRequestToolMessage {
                content: result.into(),
                tool_call_id: tc.id.clone(),
            }.into());
        }

        // Continue the conversation with tool results
        let final_response = client.complete_chat(&messages, Some(&tools)).await?;
        println!("{}", final_response.choices[0].message.content.as_deref().unwrap_or(""));
    }
}
```

Tool calling also works with streaming via `complete_streaming_chat` — accumulate tool call fragments during streaming and check for `FinishReason::ToolCalls`.

### Response Format Options

Control the output format of chat completions:

```rust
use foundry_local_sdk::ChatResponseFormat;

// Plain text (default)
let client = model.create_chat_client()
    .response_format(ChatResponseFormat::Text);

// Unstructured JSON output
let client = model.create_chat_client()
    .response_format(ChatResponseFormat::JsonObject);

// JSON constrained to a schema
let client = model.create_chat_client()
    .response_format(ChatResponseFormat::JsonSchema(r#"{
        "type": "object",
        "properties": {
            "name": { "type": "string" },
            "age": { "type": "integer" }
        },
        "required": ["name", "age"]
    }"#.to_string()));

// Output constrained by a Lark grammar (Foundry extension)
let client = model.create_chat_client()
    .response_format(ChatResponseFormat::LarkGrammar(grammar.to_string()));
```

### Audio Transcription

Transcribe audio files locally using the `AudioClient`:

```rust
let model = manager.catalog().get_model("whisper-tiny").await?;
model.load().await?;

let audio_client = model.create_audio_client()
    .language("en");

// Non-streaming transcription
let result = audio_client.transcribe("recording.wav").await?;
println!("{}", result.text);
```

#### Streaming Transcription

```rust
use tokio_stream::StreamExt;

let mut stream = audio_client.transcribe_streaming("recording.wav").await?;
while let Some(chunk) = stream.next().await {
    print!("{}", chunk?.text);
}
```

### Embedded Web Service

Start a local HTTP server that exposes an OpenAI-compatible REST API:

```rust
manager.start_web_service().await?;
let urls = manager.urls()?;
println!("Service running at: {:?}", urls);

// Any OpenAI-compatible client or tool can now connect to the endpoint.
// ...

manager.stop_web_service().await?;
```

### Chat Client Settings

All settings are configured via chainable builder methods on `ChatClient`:

| Method | Type | Description |
|--------|------|-------------|
| `temperature(v)` | `f64` | Sampling temperature (0.0–2.0; higher = more random) |
| `max_tokens(v)` | `u32` | Maximum number of tokens to generate |
| `top_p(v)` | `f64` | Nucleus sampling probability (0.0–1.0) |
| `top_k(v)` | `u32` | Top-k sampling parameter (Foundry extension) |
| `frequency_penalty(v)` | `f64` | Frequency penalty |
| `presence_penalty(v)` | `f64` | Presence penalty |
| `n(v)` | `u32` | Number of completions to generate |
| `random_seed(v)` | `u64` | Random seed for reproducible results (Foundry extension) |
| `response_format(v)` | `ChatResponseFormat` | Output format (Text, JsonObject, JsonSchema, LarkGrammar) |
| `tool_choice(v)` | `ChatToolChoice` | Tool selection strategy (None, Auto, Required, Function) |

## Error Handling

All fallible operations return `foundry_local_sdk::Result<T>`, which is an alias for `std::result::Result<T, FoundryLocalError>`.

```rust
use foundry_local_sdk::FoundryLocalError;

match manager.catalog().get_model("nonexistent").await {
    Ok(model) => { /* use model */ }
    Err(FoundryLocalError::ModelOperation { reason }) => {
        eprintln!("Model error: {reason}");
    }
    Err(FoundryLocalError::CommandExecution { reason }) => {
        eprintln!("Core engine error: {reason}");
    }
    Err(e) => {
        eprintln!("Unexpected error: {e}");
    }
}
```

### Error Variants

| Variant | Description |
|---------|-------------|
| `LibraryLoad { reason }` | The native core library could not be loaded |
| `CommandExecution { reason }` | A command executed against native core returned an error |
| `InvalidConfiguration { reason }` | The provided configuration is invalid |
| `ModelOperation { reason }` | A model operation failed (load, unload, download, etc.) |
| `HttpRequest(reqwest::Error)` | An HTTP request to an external service failed |
| `Serialization(serde_json::Error)` | JSON serialization/deserialization failed |
| `Validation { reason }` | A validation check on user-supplied input failed |
| `Io(std::io::Error)` | An I/O error occurred |

## Configuration

The SDK is configured via `FoundryLocalConfig` when creating the manager:

```rust
use foundry_local_sdk::{FoundryLocalConfig, LogLevel};

let config = FoundryLocalConfig::new("my_app")
    .log_level(LogLevel::Info)
    .model_cache_dir("/path/to/cache")
    .web_service_urls("http://127.0.0.1:5000");

let manager = FoundryLocalManager::create(config)?;
```

| Setting | Builder method | Default | Description |
|---------|---------------|---------|-------------|
| App name | `new(name)` | **(required)** | Your application name |
| App data dir | `.app_data_dir(dir)` | `~/.{app_name}` | Application data directory |
| Model cache dir | `.model_cache_dir(dir)` | `{app_data_dir}/cache/models` | Where models are stored locally |
| Logs dir | `.logs_dir(dir)` | `{app_data_dir}/logs` | Log output directory |
| Log level | `.log_level(level)` | `Warn` | `Trace`, `Debug`, `Info`, `Warn`, `Error`, `Fatal` |
| Web service URLs | `.web_service_urls(urls)` | `None` | Bind address for the embedded web service |
| Service endpoint | `.service_endpoint(url)` | `None` | URL of an existing external service to connect to |
| Library path | `.library_path(path)` | Auto-discovered | Path to native Foundry Local Core libraries |
| Additional settings | `.additional_setting(k, v)` | `None` | Extra key-value settings passed to Core |
| Logger | `.logger(impl Logger)` | `None` | Application logger (stub — not yet wired) |

## How It Works

### Native Library Download

The `build.rs` build script automatically downloads the required native libraries at compile time:

1. Queries NuGet/ORT-Nightly feeds for package metadata
2. Downloads `.nupkg` packages (zip archives)
3. Extracts platform-specific native libraries (`.dll`, `.so`, or `.dylib`)
4. Places them in Cargo's `OUT_DIR` for runtime discovery

Downloaded libraries are cached — subsequent builds skip the download step.

### Runtime Loading

At runtime, the SDK uses `libloading` to dynamically load the Foundry Local Core library and resolve function pointers. No static linking or system-wide installation is required.

## Platform Support

| Platform        | RID        | Status |
|-----------------|------------|--------|
| Windows x64     | `win-x64`  | ✅     |
| Windows ARM64   | `win-arm64`| ✅     |
| Linux x64       | `linux-x64`| ✅     |
| macOS ARM64     | `osx-arm64`| ✅     |

## Running Examples

Sample applications are available in [`samples/rust/`](../../samples/rust/):

| Sample | Description |
|--------|-------------|
| `native-chat-completions` | Non-streaming and streaming chat completions |
| `tool-calling-foundry-local` | Function/tool calling with multi-turn conversations |
| `audio-transcription-example` | Audio transcription (non-streaming and streaming) |
| `foundry-local-webserver` | Embedded OpenAI-compatible REST API server |

Run a sample with:

```sh
cd samples/rust
cargo run -p native-chat-completions
```

## License

MIT — see [LICENSE](../../LICENSE) for details.
