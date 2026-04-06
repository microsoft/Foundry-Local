# Foundry Local Playground

Interactive CLI samples showcasing the [Foundry Local](https://github.com/microsoft/Foundry-Local) SDK across four language bindings. Each sample follows the same flow — discover hardware, browse models, and run interactive inference — all on-device.

## Language Bindings

| | Language | Directory | Get Started |
|---|---|---|---|
| 🟨 | **JavaScript** | [`js/`](js/) | [README →](js/README.md) |
| 🐍 | **Python** | [`python/`](python/) | [README →](python/README.md) |
| 🟦 | **C#** | [`cs/`](cs/) | [README →](cs/README.md) |
| 🦀 | **Rust** | [`rust/`](rust/) | [README →](rust/README.md) |

## What Each Sample Does

```
┌─────────────────────────────────────────────────────────────┐
│  1. Initialize SDK                                          │
│  2. Discover & download execution providers (with progress) │
│  3. Browse model catalog → user picks a model               │
│  4. Download model (with progress) → load into memory       │
│  5. Interactive chat  ─or─  audio transcription (streaming) │
│  6. Clean up                                                │
└─────────────────────────────────────────────────────────────┘
```

The task type is detected automatically from the model metadata — chat models start an interactive conversation, and Whisper models start an audio transcription session.

## Prerequisites

- **Windows**: [Foundry Local](https://github.com/microsoft/Foundry-Local) runtime (installed automatically by the SDK on first use).
- **macOS / Linux**: Foundry Local runtime — `brew tap microsoft/foundrylocal && brew install foundrylocal`, or see the [official docs](https://learn.microsoft.com/azure/foundry-local/get-started).

## Project Structure

```
FoundryLocalPlayground/
├── js/
│   ├── app.js           # Main SDK flow
│   ├── ui.js            # Terminal UI helpers
│   └── package.json
├── python/
│   ├── sample.py        # Main SDK flow
│   └── ui.py            # Terminal UI helpers
├── cs/
│   ├── Program.cs       # Main SDK flow
│   ├── Ui.cs            # Terminal UI helpers
│   ├── FoundryPlayground.csproj
│   └── nuget.config
└── rust/
    ├── main.rs          # Main SDK flow
    ├── ui.rs            # Terminal UI helpers
    └── Cargo.toml
```

Each sample separates **SDK logic** (main file) from **terminal drawing** (UI helper), so the main file reads as a clean walkthrough of the Foundry Local API.
