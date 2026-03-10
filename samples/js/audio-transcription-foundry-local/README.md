# Sample: Audio Transcription with Foundry Local

This sample demonstrates how to use Foundry Local for **speech-to-text (audio transcription)** using the Whisper model — entirely on-device, with no cloud services required.

## What This Shows

- Loading the `whisper-tiny` model via the Foundry Local SDK
- Transcribing an audio file (`.wav`, `.mp3`, etc.) to text
- Both standard and streaming transcription modes
- Automatic hardware acceleration (NPU > GPU > CPU)

## Prerequisites

- [Foundry Local](https://github.com/microsoft/Foundry-Local) installed on your machine
- Node.js 18+

## Getting Started

Install the Foundry Local SDK:

```bash
npm install foundry-local-sdk
```

Place an audio file (e.g., `recording.wav` or `recording.mp3`) in the project directory, then run:

```bash
node src/app.js
```

## How It Works

The Foundry Local SDK handles everything:
1. **Model discovery** — finds the best `whisper-tiny` variant for your hardware
2. **Model download** — downloads the model if not already cached
3. **Model loading** — loads the model into memory with optimized hardware acceleration
4. **Transcription** — runs Whisper inference entirely on-device

No need for `whisper.cpp`, `@huggingface/transformers`, or any other separate STT tool.
