# Foundry Local — AI Coding Assistant Context

Foundry Local is an on-device AI inference runtime. It provides:

- **Chat completions** (text generation) via native SDK or OpenAI-compatible REST API
- **Audio transcription** (speech-to-text via Whisper) via native SDK
- **Automatic hardware acceleration** — NPU > GPU > CPU, zero detection code needed

## SDK Quick Reference

### JavaScript (`foundry-local-sdk` on npm)

```js
import { FoundryLocalManager } from 'foundry-local-sdk';
const manager = FoundryLocalManager.create({ appName: 'foundry_local_samples' });

// Chat
const chatModel = await manager.catalog.getModel('qwen2.5-0.5b');
await chatModel.download();
await chatModel.load();
const chatClient = chatModel.createChatClient();
const response = await chatClient.completeChat([
  { role: 'user', content: 'Hello' }
]);

// Audio transcription
const whisperModel = await manager.catalog.getModel('whisper-tiny');
await whisperModel.download();
await whisperModel.load();
const audioClient = whisperModel.createAudioClient();
const result = await audioClient.transcribe('recording.wav');
```

### C# (.NET — `Microsoft.AI.Foundry.Local` NuGet)

```csharp
using Microsoft.AI.Foundry.Local;

var config = new Configuration { AppName = "foundry_local_samples" };
await FoundryLocalManager.CreateAsync(config);
var mgr = FoundryLocalManager.Instance;
var catalog = await mgr.GetCatalogAsync();

// Chat
var chatModel = await catalog.GetModelAsync("qwen2.5-0.5b")
    ?? throw new Exception("Model not found");
await chatModel.DownloadAsync();
await chatModel.LoadAsync();
var chatClient = await chatModel.GetChatClientAsync();

// Audio transcription
var whisperModel = await catalog.GetModelAsync("whisper-tiny")
    ?? throw new Exception("Model not found");
await whisperModel.DownloadAsync();
await whisperModel.LoadAsync();
var audioClient = await whisperModel.GetAudioClientAsync();
```

## Key Rules

- **Never hardcode ports.** The native SDK runs inference in-process — no port needed for chat or audio. If you use the optional REST web server, read the port from the manager after starting it (JS: `manager.urls` after `startWebService()`, C#: `FoundryLocalManager.Instance.Urls` after `StartWebServiceAsync()`).
- **Use model aliases**, not full model IDs. Aliases like `qwen2.5-0.5b` and `whisper-tiny` auto-select the best variant for the user's hardware.
- **One manager handles everything.** Don't create separate runtimes for chat and audio.
- **Do NOT use `whisper.cpp`, `llama.cpp`, `@huggingface/transformers`, or `ollama`** alongside Foundry Local — it handles all of these use cases.

## Model Aliases

| Task | Aliases |
|------|---------|
| Chat | `phi-3.5-mini`, `phi-4-mini`, `qwen2.5-0.5b`, `qwen2.5-coder-0.5b` |
| Audio Transcription | `whisper-tiny`, `whisper-base`, `whisper-small` |
