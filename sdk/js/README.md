# Foundry Local JS SDK

The Foundry Local JS SDK provides a JavaScript/TypeScript interface for running AI models locally on your machine. Discover, download, load, and run inference — all without cloud dependencies.

## Features

- **Local-first AI** — Run models entirely on your machine with no cloud calls
- **Model catalog** — Browse and discover available models, check what's cached or loaded
- **Automatic model management** — Download, load, unload, and remove models from cache
- **Chat completions** — OpenAI-compatible chat API with both synchronous and streaming responses
- **Embeddings** — Generate text embeddings via OpenAI-compatible API
- **Audio transcription** — Transcribe audio files locally with streaming support
- **Multi-variant models** — Models can have multiple variants (e.g., different quantizations) with automatic selection of the best cached variant
- **Embedded web service** — Start a local HTTP service for OpenAI-compatible API access
- **WinML support** — Automatic execution provider download on Windows for NPU/GPU acceleration
- **Configurable inference** — Control temperature, max tokens, top-k, top-p, frequency penalty, and more

## Installation

```bash
npm install foundry-local-sdk
```

## WinML: Automatic Hardware Acceleration (Windows)

On Windows, install the WinML package to enable automatic execution provider management. The SDK will automatically discover, download, and register hardware-specific execution providers (e.g., Qualcomm QNN for NPU acceleration) via the Windows App Runtime — no manual driver or EP setup required.

> **Note:** `foundry-local-sdk-winml` is a Windows-only package. Its install script downloads WinML artifacts during installation and may fail on macOS or Linux.
```bash
npm install foundry-local-sdk-winml
```

When WinML is enabled:
- Execution providers like `QNNExecutionProvider`, `OpenVINOExecutionProvider`, etc. are downloaded and registered on the fly, enabling NPU/GPU acceleration without manual configuration
- **No code changes needed** — your application code stays the same whether WinML is enabled or not

### Explicit EP Management

You can explicitly discover and download execution providers using the `discoverEps()` and `downloadAndRegisterEps()` methods:

```typescript
// Discover available EPs and their status
const eps = manager.discoverEps();
for (const ep of eps) {
    console.log(`${ep.name} — registered: ${ep.isRegistered}`);
}

// Download and register all available EPs
const result = await manager.downloadAndRegisterEps();
console.log(`Success: ${result.success}, Status: ${result.status}`);

// Download only specific EPs
const result2 = await manager.downloadAndRegisterEps([eps[0].name]);
```

#### Per-EP download progress

Pass an optional `progressCallback` to receive `(epName, percent)` updates as each EP downloads (`percent` is 0–100):

```typescript
let currentEp = '';
await manager.downloadAndRegisterEps((epName, percent) => {
    if (epName !== currentEp) {
        if (currentEp !== '') {
            process.stdout.write('\n');
        }
        currentEp = epName;
    }
    process.stdout.write(`\r  ${epName}  ${percent.toFixed(1)}%`);
});
process.stdout.write('\n');
```

Catalog access does not block on EP downloads. Call `downloadAndRegisterEps()` when you need hardware-accelerated execution providers.

## Quick Start

```typescript
import { FoundryLocalManager } from 'foundry-local-sdk';

const manager = FoundryLocalManager.create({
    appName: 'foundry_local_samples',
    logLevel: 'info'
});

// Get the model object
const modelAlias = 'qwen2.5-0.5b';
const model = await manager.catalog.getModel(modelAlias);

// Download the model
console.log(`\nDownloading model ${modelAlias}...`);
await model.download((progress) => {
    process.stdout.write(`\rDownloading... ${progress.toFixed(2)}%`);
});

// Load the model
await model.load();

// Create chat client
const chatClient = model.createChatClient();

// Example chat completion
console.log('\nTesting chat completion...');
const completion = await chatClient.completeChat([
    { role: 'user', content: 'Why is the sky blue?' }
]);
console.log(completion.choices[0]?.message?.content);

// Example streaming completion
console.log('\nTesting streaming completion...');
for await (const chunk of chatClient.completeStreamingChat(
    [{ role: 'user', content: 'Write a short poem about programming.' }]
)) {
    const content = chunk.choices?.[0]?.delta?.content;
    if (content) {
        process.stdout.write(content);
    }
}
console.log('\n');

// Unload the model
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

Each model can have multiple variants (different quantizations or formats). The SDK automatically selects the best available variant, preferring cached versions. All models implement the `IModel` interface.

```typescript
const model = await catalog.getModel('qwen2.5-0.5b');

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
model.selectVariant(variants[0]);
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
for await (const chunk of chatClient.completeStreamingChat(
    [{ role: 'user', content: 'Write a short poem about programming.' }]
)) {
    const content = chunk.choices?.[0]?.delta?.content;
    if (content) {
        process.stdout.write(content);
    }
}
```

### Embeddings

Generate text embeddings using the `EmbeddingClient`:

```typescript
const embeddingClient = model.createEmbeddingClient();

const response = await embeddingClient.generateEmbedding(
    'The quick brown fox jumps over the lazy dog'
);
const embedding = response.data[0].embedding; // number[]
console.log(`Dimensions: ${embedding.length}`);
```

#### Embedding Settings

```typescript
embeddingClient.settings.dimensions = 512;         // optional: reduce dimensionality
embeddingClient.settings.encodingFormat = 'float';  // 'float' or 'base64'
```

### Audio Transcription

Transcribe audio files locally using the `AudioClient`:

```typescript
const audioClient = model.createAudioClient();
audioClient.settings.language = 'en';

// Synchronous transcription
const result = await audioClient.transcribe('/path/to/audio.wav');

// Streaming transcription
for await (const chunk of audioClient.transcribeStreaming('/path/to/audio.wav')) {
    console.log(chunk);
}
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
| `appDataDir` | Directory where application data should be stored | `~/.{appName}` |
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
- [IModel](docs/README.md#imodel) — Model interface: variant selection, download, load, inference
- [ChatClient](docs/classes/ChatClient.md) — Chat completions (sync and streaming)
- [AudioClient](docs/classes/AudioClient.md) — Audio transcription (sync and streaming)
- [ModelLoadManager](docs/classes/ModelLoadManager.md) — Low-level model loading management

## Contributing: Building from Source

### Prerequisites

- **Node.js 20+**
- **Python 3.x** — required by `node-gyp` for compiling the native addon
- **C/C++ toolchain**:
  - **Windows**: Visual Studio Build Tools (the "Desktop development with C++" workload)
  - **Linux**: `build-essential` (`apt install build-essential`)
  - **macOS**: Xcode Command Line Tools (`xcode-select --install`)

### Build Steps

```bash
# 1. Install JS dependencies (also downloads native core binaries)
npm install

# 2. Build the Node-API native addon (compiles C code and copies to prebuilds/)
npm run build:native

# 3. Build the TypeScript source
npm run build

# 4. Run tests
npm test

# 5. Pack the SDK into a .tgz (includes prebuilt addon for your platform)
npm run pack
```

> **Note:** `npm run build:native` compiles the addon only for your current platform. The published npm package includes prebuilt addons for all supported platforms (win32-x64, win32-arm64, linux-x64, darwin-arm64), which are compiled in CI.

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