# Foundry Local JS SDK

The Foundry Local JS SDK provides a JavaScript/TypeScript interface for running AI models locally on your machine. Discover, download, load, and run inference — all without cloud dependencies.

## Features

- **Local-first AI** — Run models entirely on your machine with no cloud calls
- **Model catalog** — Browse and discover available models, check what's cached or loaded
- **Automatic model management** — Download, load, unload, and remove models from cache
- **Chat completions** — OpenAI-compatible chat API with both synchronous and streaming responses
- **Audio transcription** — Transcribe audio files locally with streaming support
- **Multi-variant models** — Models can have multiple variants (e.g., different quantizations) with automatic selection of the best cached variant
- **Embedded web service** — Start a local HTTP service for OpenAI-compatible API access
- **WinML support** — Automatic execution provider download on Windows for NPU/GPU acceleration
- **Configurable inference** — Control temperature, max tokens, top-k, top-p, frequency penalty, and more

## Installation

```bash
npm install @prathikrao/foundry-local-sdk
```

## WinML: Automatic Hardware Acceleration (Windows)

On Windows, install with the `--winml` flag to enable automatic execution provider management. The SDK will automatically discover, download, and register hardware-specific execution providers (e.g., Qualcomm QNN for NPU acceleration) via the Windows App Runtime — no manual driver or EP setup required.

```bash
npm install @prathikrao/foundry-local-sdk --winml
```

When WinML is enabled:
- Execution providers like `QNNExecutionProvider`, `OpenVINOExecutionProvider`, etc. are downloaded and registered on the fly, enabling NPU/GPU acceleration without manual configuration
- **No code changes needed** — your application code stays the same whether WinML is enabled or not

> **Note:** The `--winml` flag is only relevant on Windows. On macOS and Linux, the standard installation is used regardless of this flag.

## Quick Start

```typescript
import { FoundryLocalManager } from '@prathikrao/foundry-local-sdk';

// Initialize the SDK
const manager = FoundryLocalManager.create({
    appName: 'MyApp',
    logLevel: 'info'
});

// Get a model from the catalog
const model = await manager.catalog.getModel('phi-3-mini');

// Load the model into memory
await model.load();

// Run a chat completion
const chatClient = model.createChatClient();
const response = await chatClient.completeChat([
    { role: 'user', content: 'Hello, how are you?' }
]);
console.log(response.choices[0].message.content);

// Clean up
await model.unload();
```

## Usage

### Browsing the Model Catalog

The `Catalog` lets you discover what models are available, which are already cached locally, and which are currently loaded in memory.

```typescript
const catalog = manager.catalog;

// List all available models
const models = await catalog.getModels();
models.forEach(model => {
    console.log(`${model.alias} — cached: ${model.isCached}`);
});

// See what's already downloaded
const cached = await catalog.getCachedModels();

// See what's currently loaded in memory
const loaded = await catalog.getLoadedModels();
```

### Loading and Running Models

Each `Model` can have multiple variants (different quantizations or formats). The SDK automatically selects the best available variant, preferring cached versions.

```typescript
const model = await catalog.getModel('phi-3-mini');

// Download if not cached (with optional progress tracking)
if (!model.isCached) {
    await model.download((progress) => {
        console.log(`Download: ${progress}%`);
    });
}

// Load into memory and run inference
await model.load();
const chatClient = model.createChatClient();
```

You can also select a specific variant manually:

```typescript
const variants = model.variants;
model.selectVariant(variants[0].id);
```

### Chat Completions

The `ChatClient` follows the OpenAI Chat Completion API structure.

```typescript
const chatClient = model.createChatClient();

// Configure settings
chatClient.settings.temperature = 0.7;
chatClient.settings.maxTokens = 800;
chatClient.settings.topP = 0.9;

// Synchronous completion
const response = await chatClient.completeChat([
    { role: 'system', content: 'You are a helpful assistant.' },
    { role: 'user', content: 'Explain quantum computing in simple terms.' }
]);
console.log(response.choices[0].message.content);
```

### Streaming Responses

For real-time output, use streaming:

```typescript
await chatClient.completeStreamingChat(
    [{ role: 'user', content: 'Write a short poem about programming.' }],
    (chunk) => {
        const content = chunk.choices?.[0]?.message?.content;
        if (content) {
            process.stdout.write(content);
        }
    }
);
```

### Audio Transcription

Transcribe audio files locally using the `AudioClient`:

```typescript
const audioClient = model.createAudioClient();
audioClient.settings.language = 'en';

// Synchronous transcription
const result = await audioClient.transcribe('/path/to/audio.wav');

// Streaming transcription
await audioClient.transcribeStreaming('/path/to/audio.wav', (chunk) => {
    console.log(chunk);
});
```

### Embedded Web Service

Start a local HTTP server that exposes an OpenAI-compatible API:

```typescript
manager.startWebService();
console.log('Service running at:', manager.urls);

// Use with any OpenAI-compatible client library
// ...

manager.stopWebService();
```

### Configuration

The SDK is configured via `FoundryLocalConfig` when creating the manager:

| Option | Description | Default |
|--------|-------------|---------|
| `appName` | **Required.** Application name for logs and telemetry. | — |
| `logLevel` | Logging level: `trace`, `debug`, `info`, `warn`, `error`, `fatal` | `warn` |
| `modelCacheDir` | Directory for downloaded models | `~/.{appName}/cache/models` |
| `logsDir` | Directory for log files | `~/.{appName}/logs` |
| `libraryPath` | Path to native Foundry Local Core libraries | Auto-discovered |
| `serviceEndpoint` | URL of an existing external service to connect to | — |
| `webServiceUrls` | URL(s) for the embedded web service to bind to | — |

## API Reference

Auto-generated class documentation lives in [`docs/classes/`](docs/classes/):

- [FoundryLocalManager](docs/classes/FoundryLocalManager.md) — SDK entry point, web service management
- [Catalog](docs/classes/Catalog.md) — Model discovery and browsing
- [Model](docs/classes/Model.md) — High-level model with variant selection
- [ModelVariant](docs/classes/ModelVariant.md) — Specific model variant: download, load, inference
- [ChatClient](docs/classes/ChatClient.md) — Chat completions (sync and streaming)
- [AudioClient](docs/classes/AudioClient.md) — Audio transcription (sync and streaming)
- [ModelLoadManager](docs/classes/ModelLoadManager.md) — Low-level model loading management

## Running Tests

```bash
npm test
```

See `test/README.md` for details on prerequisites and setup.

## Running Examples

```bash
npm run example
```

This runs the chat completion example in `examples/chat-completion.ts`.