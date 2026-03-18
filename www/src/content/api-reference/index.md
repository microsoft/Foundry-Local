---
title: API Reference
description: Complete API reference for the Foundry Local SDKs. Build AI-powered applications with JavaScript/TypeScript or C#/.NET.
---

# API Reference

Foundry Local provides native SDKs for building AI-powered applications that run entirely on-device. Both SDKs share the same architecture and concepts, so code patterns transfer across languages.

## Available SDKs

| SDK | Package | Install |
|-----|---------|---------|
| **JavaScript / TypeScript** | `foundry-local-sdk` | `npm install foundry-local-sdk` |
| **C# / .NET** | `Microsoft.AI.Foundry.Local` | `dotnet add package Microsoft.AI.Foundry.Local` |

## Core Concepts

Every Foundry Local application follows the same workflow regardless of language:

1. **Initialize** — Create a `FoundryLocalManager` with your app configuration
2. **Discover** — Use the `Catalog` to browse available models
3. **Download** — Pull a model to your local cache (with progress tracking)
4. **Load** — Load the model into memory for inference
5. **Infer** — Create a `ChatClient` or `AudioClient` and run completions or transcriptions
6. **Cleanup** — Unload models when done to free resources

## Architecture Overview

Both SDKs expose the same class hierarchy:

- **FoundryLocalManager** — Singleton entry point. Initializes the native core, provides access to the catalog and optional web service.
- **Catalog** — Discovers and lists models from the model registry. Returns `Model` objects grouped by alias.
- **Model** — A logical model (e.g. "qwen2.5-0.5b") that may have multiple variants (quantizations, device targets). Delegates lifecycle operations to its selected variant.
- **ModelVariant** — A specific model binary (e.g. "qwen2.5-0.5b-cpu-int4"). Handles download, load, unload, and cache operations.
- **ChatClient** — Performs chat completions (synchronous and streaming) against a loaded model. Follows the OpenAI Chat Completions API structure.
- **AudioClient** — Performs audio transcription (synchronous and streaming) against a loaded model. Follows the OpenAI Audio API structure.
- **Configuration** — Specifies app name, directories, log level, and optional web service settings.

## Quick Comparison

### Initialization

**JavaScript:**

```typescript
import { FoundryLocalManager } from 'foundry-local-sdk';

const manager = FoundryLocalManager.create({
  appName: 'MyApp',
  logLevel: 'info'
});
```

**C#:**

```csharp
using Microsoft.AI.Foundry.Local;

var config = new Configuration { AppName = "MyApp", LogLevel = LogLevel.Information };
await FoundryLocalManager.CreateAsync(config, logger);
var manager = FoundryLocalManager.Instance;
```

### Chat Completion

**JavaScript:**

```typescript
const model = await manager.catalog.getModel('qwen2.5-0.5b');
await model.download();
await model.load();

const chat = model.createChatClient();
const response = await chat.completeChat([
  { role: 'user', content: 'Hello!' }
]);
```

**C#:**

```csharp
var catalog = await manager.GetCatalogAsync();
var model = await catalog.GetModelAsync("qwen2.5-0.5b");
await model.DownloadAsync();
await model.LoadAsync();

var chat = await model.GetChatClientAsync();
var response = await chat.CompleteChatAsync(new[] {
    ChatMessage.FromUser("Hello!")
});
```

## Detailed References

- [JavaScript / TypeScript API Reference](/docs/api-reference/javascript) — Full class and method documentation for the JS SDK
- [C# / .NET API Reference](/docs/api-reference/csharp) — Full class and method documentation for the C# SDK

## Source Code

- [JavaScript SDK on GitHub](https://github.com/microsoft/Foundry-Local/tree/main/sdk_v2/js)
- [C# SDK on GitHub](https://github.com/microsoft/Foundry-Local/tree/main/sdk_v2/cs)
- [JavaScript Samples](https://github.com/microsoft/Foundry-Local/tree/main/samples/js)
- [C# Samples](https://github.com/microsoft/Foundry-Local/tree/main/samples/cs)
