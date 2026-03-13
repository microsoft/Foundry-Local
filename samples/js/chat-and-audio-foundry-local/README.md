# Sample: Chat + Audio Transcription with Foundry Local

This sample demonstrates how to use Foundry Local as a **unified AI runtime** for both **text generation (chat)** and **speech-to-text (audio transcription)** — all on-device, with a single SDK managing both models.

## What This Shows

- Using a single `FoundryLocalManager` to manage both chat and audio models
- Transcribing an audio file using the `whisper-tiny` model
- Analyzing the transcription using the `phi-3.5-mini` chat model
- Automatic hardware acceleration for both models — zero hardware detection code needed

## Why Foundry Local?

Without Foundry Local, building an app with both chat and speech-to-text typically requires:
- A separate STT library (`whisper.cpp`, `@huggingface/transformers`)
- A separate LLM runtime (`llama.cpp`, `node-llama-cpp`)
- Custom hardware detection code for each runtime (~200+ lines)
- Separate model download and caching logic

With Foundry Local, you get **one SDK, one service, both capabilities** — and the hardware detection is automatic.

## Prerequisites

- [Foundry Local](https://github.com/microsoft/Foundry-Local) installed on your machine
- Node.js 18+

## Getting Started

Install the Foundry Local SDK:

```bash
npm install foundry-local-sdk
```

Place an audio file (`recording.mp3`) in the project directory, then run:

```bash
node src/app.js
```
