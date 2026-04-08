# Foundry Local Playground — Rust

> [JavaScript](../js/README.md) · [Python](../python/README.md) · [C#](../cs/README.md) · **[Rust](.)**

An interactive CLI sample that demonstrates the full Foundry Local Rust SDK — from hardware discovery to streaming inference.

## Prerequisites

- [Rust](https://www.rust-lang.org/tools/install) 1.70+ (stable toolchain)
- An internet connection on first build (to download native libraries)
- An Azure DevOps PAT with **Packaging > Read** scope on the `aiinfra` organization

## Setup

The `foundry-local-sdk` crate is hosted on an Azure Artifacts feed. Before building, set the registry token as an environment variable in your terminal:

```powershell
$pat = "<YOUR_ADO_PAT>"
$token = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes(":$pat"))
$env:CARGO_REGISTRIES_AIFOUNDRYLOCAL_TOKEN = "Basic $token"
```

Replace `<YOUR_ADO_PAT>` with an Azure DevOps PAT that has **Packaging > Read** scope on the `aiinfra` organization.

> To avoid repeating this each session, add the above to your PowerShell profile or set `CARGO_REGISTRIES_AIFOUNDRYLOCAL_TOKEN` as a persistent environment variable.

## Build & Run

```bash
cd rust
cargo run
```

> **Note:** The first build downloads native Foundry Local Core libraries — this may take a minute. Subsequent builds are cached.

The `Cargo.toml` enables the `winml` feature for Windows hardware acceleration. On macOS/Linux, remove the feature flag:

```toml
# Cross-platform (no WinML)
foundry-local-sdk = { version = "1.0.0-rc3", registry = "aifoundrylocal" }
```

## What Happens

1. **Execution providers** are discovered and downloaded with live progress bars.
2. A **model catalog** table is displayed — enter a number to pick a model.
3. The model is **downloaded** (if needed) and **loaded** into memory.
4. Depending on the model type:
   - **Chat models** → interactive conversation with streaming token output in bordered boxes.
   - **Whisper models** → audio transcription — enter a `.wav`/`.mp3` file path and see the transcript stream in.
5. Type `/quit` to exit.

## File Overview

| File | Purpose |
|---|---|
| `main.rs` | Main SDK flow — initialize, discover EPs, browse catalog, load model, run inference |
| `ui.rs` | Terminal UI helpers — progress bars, box-drawing tables, streaming output boxes |
| `Cargo.toml` | Cargo manifest with dependencies |

## Key SDK APIs Used

```rust
use foundry_local_sdk::{
    FoundryLocalConfig, FoundryLocalManager,
    ChatCompletionRequestMessage, ChatCompletionRequestSystemMessage,
    ChatCompletionRequestUserMessage,
};
use tokio_stream::StreamExt;

// Initialize
let manager = FoundryLocalManager::create(FoundryLocalConfig::new("my_app"))?;

// Execution providers
let eps = manager.discover_eps()?;
manager.download_and_register_eps_with_progress(None, |name, pct| { ... }).await?;

// Model catalog
let catalog = manager.catalog();
let models  = catalog.get_models().await?;
let model   = catalog.get_model("phi-3.5-mini").await?;
model.select_variant("phi-3.5-mini-generic-gpu-4")?;  // by variant ID

// Download & load
model.download(Some(Box::new(|progress: &str| { ... }))).await?;
model.load().await?;

// Chat inference (streaming)
let client = model.create_chat_client().temperature(0.7).max_tokens(512);
let mut stream = client.complete_streaming_chat(&messages, None).await?;
while let Some(chunk) = stream.next().await {
    let chunk = chunk?;
    if let Some(content) = &chunk.choices[0].delta.content {
        print!("{content}");
    }
}

// Audio transcription (streaming)
let audio = model.create_audio_client().language("en");
let mut stream = audio.transcribe_streaming("file.wav").await?;
while let Some(chunk) = stream.next().await {
    print!("{}", chunk?.text);
}

// Cleanup
model.unload().await?;
```

## Screenshot

```
────────────────────────────────────────────────────────────────────
  Execution Providers
────────────────────────────────────────────────────────────────────
  ┌──────────────────────┬───────────────────────────────────────┐
  │ EP Name              │ Status                                │
  ├──────────────────────┼───────────────────────────────────────┤
  │ QNNExecutionProvider │ ● registered                         │
  │ CPUExecutionProvider │ ● registered                         │
  └──────────────────────┴───────────────────────────────────────┘

────────────────────────────────────────────────────────────────────
  Model – phi-3.5-mini
────────────────────────────────────────────────────────────────────
  ┌──────────────┬───────────────────────────────────────┐
  │ Model        │ Progress                              │
  ├──────────────┼───────────────────────────────────────┤
  │ phi-3.5-mini │ ██████████████████████████████ done    │
  └──────────────┴───────────────────────────────────────┘
  ✓ Model loaded
```
