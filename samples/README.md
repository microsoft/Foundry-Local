# Foundry Local Samples

Explore complete working examples that demonstrate how to use Foundry Local — an end-to-end local AI solution that runs entirely on-device. These samples cover chat completions, audio transcription, tool calling, LangChain integration, and more.

> **New to Foundry Local?** Check out the [main README](../README.md) for an overview and quickstart, or visit the [Foundry Local documentation](https://learn.microsoft.com/en-us/azure/foundry-local/) on Microsoft Learn.

## Samples by Language

| Language | Samples | Description |
|----------|---------|-------------|
| [**C#**](cs/) | 12 | .NET SDK samples including native chat, audio transcription, tool calling, model management, web server, and tutorials. Uses WinML on Windows for hardware acceleration. |
| [**JavaScript**](js/) | 12 | Node.js SDK samples including native chat, audio transcription, Electron desktop app, Copilot SDK integration, LangChain, tool calling, web server, and tutorials. |
| [**Python**](python/) | 9 | Python samples using the OpenAI-compatible API, including chat, audio transcription, LangChain integration, tool calling, web server, and tutorials. |
| [**Rust**](rust/) | 8 | Rust SDK samples including native chat, audio transcription, tool calling, web server, and tutorials. |

## Common Patterns

Most samples follow a similar pattern:

1. **Initialize** the Foundry Local manager
2. **Download** a model (auto-selects the best variant for your hardware)
3. **Load** the model into memory
4. **Run inference** (chat completions or audio transcription)
5. **Unload** the model when done

## Models Used

| Model | Task | Used In |
|-------|------|---------|
| `qwen2.5-0.5b` | Chat / Text Generation | Most chat and tool-calling samples |
| `phi-3.5-mini` | Chat / Text Generation | Some Python samples |
| `whisper-tiny` | Audio Transcription | All audio/voice samples |
