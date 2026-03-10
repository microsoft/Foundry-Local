# 🎓 Foundry Local Documentation

Documentation for Foundry Local can be found in the following resources:

- [Microsoft Learn](https://learn.microsoft.com/azure/foundry-local/): This is the official documentation for Foundry Local, providing comprehensive guides, tutorials, and reference materials to help you get started and make the most of Foundry Local.
- SDK Reference:
    - [C# SDK Reference](../sdk_v2/cs/README.md): This documentation provides detailed information about the C# SDK for Foundry Local, including API references, usage examples, and best practices for integrating Foundry Local into your applications.
    - [JavaScript SDK Reference](../sdk_v2/js/README.md): This documentation offers detailed information about the JavaScript SDK for Foundry Local, including API references, usage examples, and best practices for integrating Foundry Local into your web applications.

## Supported Capabilities

Foundry Local is a unified local AI runtime that supports both **text generation** and **speech-to-text** through a single SDK:

| Capability | Model Aliases | SDK API |
|------------|--------------|---------|
| Chat Completions (Text Generation) | `phi-3.5-mini`, `qwen2.5-0.5b`, etc. | `model.createChatClient()` |
| Audio Transcription (Speech-to-Text) | `whisper-tiny` | `model.createAudioClient()` |

## Samples

- [JavaScript: Chat (Hello Foundry Local)](../samples/js/hello-foundry-local/) — Basic chat completions
- [JavaScript: Audio Transcription](../samples/js/audio-transcription-foundry-local/) — Speech-to-text with Whisper
- [JavaScript: Chat + Audio](../samples/js/chat-and-audio-foundry-local/) — Unified chat and audio in one app
- [JavaScript: Tool Calling](../samples/js/tool-calling-foundry-local/) — Function calling with local models
- [C#: Getting Started](../samples/cs/GettingStarted/) — C# SDK examples including audio transcription