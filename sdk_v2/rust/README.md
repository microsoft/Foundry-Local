# Foundry Local Rust SDK

Rust bindings for [Foundry Local](https://github.com/microsoft/Foundry-Local) — run AI models locally with a simple API.

The SDK dynamically loads the Foundry Local Core native engine and exposes a safe Rust interface for model management, chat completions, and audio processing.

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

## Feature Flags

| Feature   | Description |
|-----------|-------------|
| `winml`   | Use the WinML backend (Windows only). Selects different ONNX Runtime and GenAI packages. |
| `nightly` | Resolve the latest nightly build of the Core package from the ORT-Nightly feed. |

Enable features in `Cargo.toml`:

```toml
[dependencies]
foundry-local-sdk = { version = "0.1", features = ["winml"] }
```

## Quick Start

```rust
use foundry_local_sdk::{
    ChatCompletionRequestMessage, ChatCompletionRequestSystemMessage,
    ChatCompletionRequestUserMessage, FoundryLocalConfig, FoundryLocalManager,
};

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Initialize the manager — loads native libraries and starts the engine
    let manager = FoundryLocalManager::create(FoundryLocalConfig::new("my_app"))?;

    // List available models
    let models = manager.catalog().get_models().await?;
    for model in &models {
        println!("{} (id: {})", model.alias(), model.id());
    }

    // Pick a model and ensure it is loaded
    let model = manager.catalog().get_model("phi-3.5-mini").await?;
    model.load().await?;

    // Create a chat client and send a completion request
    let mut client = model.create_chat_client();
    client.temperature(0.7).max_tokens(256);

    let messages: Vec<ChatCompletionRequestMessage> = vec![
        ChatCompletionRequestSystemMessage::from("You are a helpful assistant.").into(),
        ChatCompletionRequestUserMessage::from("What is the capital of France?").into(),
    ];

    let response = client.complete_chat(&messages, None).await?;
    if let Some(choice) = response.choices.first() {
        if let Some(ref content) = choice.message.content {
            println!("{content}");
        }
    }

    Ok(())
}
```

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

## License

MIT — see [LICENSE](../../LICENSE) for details.
