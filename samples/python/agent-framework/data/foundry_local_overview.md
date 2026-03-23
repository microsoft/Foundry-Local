# Foundry Local Overview

Foundry Local is a lightweight runtime that lets developers run AI models directly on their
local machine — no cloud connection required. It is part of the Microsoft AI Foundry family
and designed for offline-first development, edge scenarios, and privacy-sensitive workloads.

## Key Features

- **Local-first inference** — models run on your device using CPU or GPU acceleration.
- **Model catalog** — browse and download curated models (Phi, Mistral, Qwen, etc.) via the SDK.
- **Cache management** — models are cached locally after the first download. The SDK exposes
  helpers to check cache status before downloading, giving apps instant startup on repeat runs.
- **OpenAI-compatible endpoint** — Foundry Local exposes a REST API compatible with the
  OpenAI Chat Completions spec, so existing OpenAI SDK code works with minimal changes.
- **Multi-language SDKs** — official SDKs for Python, JavaScript/TypeScript, C#, and Rust.

## Architecture

```
┌──────────────┐        ┌──────────────────────┐
│  Your App    │──SDK──▶│   Foundry Local       │
│  (Python,    │        │   Service (REST API)  │
│   JS, C#,    │        │                       │
│   Rust)      │        │   ┌────────────────┐  │
│              │  HTTP   │   │  Loaded Model  │  │
│              │◀───────│   │  (ONNX / GGUF) │  │
└──────────────┘        │   └────────────────┘  │
                        └──────────────────────┘
```

The SDK handles service bootstrapping, model resolution, downloading, loading, and exposes
convenience methods for chat completions and audio transcription.

## Typical Lifecycle

1. **Bootstrap** — `FoundryLocalManager` starts (or connects to) the local service.
2. **Model resolution** — the SDK resolves an alias (e.g. `phi-4-mini`) to a specific variant.
3. **Cache check** — if the model is already cached, loading is near-instant.
4. **Download** — if not cached, the model is downloaded with progress feedback.
5. **Load** — the model is loaded into the inference engine.
6. **Inference** — your app sends chat messages and receives completions.
7. **Cleanup** — unload models and stop the service when done.
