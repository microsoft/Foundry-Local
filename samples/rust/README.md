# Foundry Local Rust Samples

This directory contains samples demonstrating how to use the Foundry Local Rust SDK.

## Prerequisites

- Rust 1.70.0 or later

## Samples

### [Foundry Local Web Server](./foundry-local-webserver)

Demonstrates how to start a local OpenAI-compatible web server using the SDK, then call it with a standard HTTP client.

### [Native Chat Completions](./native-chat-completions)

Shows both non-streaming and streaming chat completions using the SDK's native chat client.

### [Tool Calling with Foundry Local](./tool-calling-foundry-local)

Demonstrates tool calling with streaming responses, multi-turn conversation, and local tool execution.

### [Audio Transcription](./audio-transcription-example)

Demonstrates audio transcription (non-streaming and streaming) using the `whisper` model.

### [Live Audio Transcription](./live-audio-transcription-example)

Demonstrates real-time microphone transcription using the `nemotron` model.
