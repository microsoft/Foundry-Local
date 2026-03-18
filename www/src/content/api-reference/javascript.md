---
title: JavaScript / TypeScript API Reference
description: Complete API reference for the Foundry Local JavaScript SDK. Covers FoundryLocalManager, Catalog, Model, ChatClient, AudioClient, and configuration.
---

# JavaScript / TypeScript API Reference

Package: `foundry-local-sdk`

```bash
npm install foundry-local-sdk
```

All classes are exported from the package root:

```typescript
import {
  FoundryLocalManager,
  Catalog,
  Model,
  ModelVariant,
  ChatClient,
  AudioClient
} from 'foundry-local-sdk';
```

---

## FoundryLocalManager

The singleton entry point for the Foundry Local SDK. Manages initialization of the native core and provides access to the Catalog and web service.

### Static Methods

#### `FoundryLocalManager.create(config)`

Creates and returns the singleton `FoundryLocalManager` instance.

| Parameter | Type | Description |
|-----------|------|-------------|
| `config` | `FoundryLocalConfig` | Configuration object. `appName` is required. |

**Returns:** `FoundryLocalManager`

```typescript
const manager = FoundryLocalManager.create({
  appName: 'MyApp',
  logLevel: 'info'
});
```

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `catalog` | `Catalog` | The Catalog instance for discovering and managing models. |
| `urls` | `string[]` | URLs where the web service is listening. Empty if web service is not running. |

### Methods

#### `startWebService()`

Starts the local OpenAI-compatible web service. After calling this, use the `urls` property to get the bound addresses. If no listener address is configured, defaults to `127.0.0.1:0` (random ephemeral port).

**Returns:** `void`

**Throws:** `Error` if starting the service fails.

#### `stopWebService()`

Stops the local web service.

**Returns:** `void`

**Throws:** `Error` if stopping the service fails.

---

## FoundryLocalConfig

Configuration object passed to `FoundryLocalManager.create()`.

| Property | Type | Required | Default | Description |
|----------|------|----------|---------|-------------|
| `appName` | `string` | **Yes** | — | Your application name. Used to derive default paths. |
| `logLevel` | `string` | No | `'warning'` | Log level: `'trace'`, `'debug'`, `'info'`, `'warn'`, `'error'`, or `'fatal'`. |
| `appDataDir` | `string` | No | `~/.{appName}` | Application data directory. |
| `modelCacheDir` | `string` | No | `{appDataDir}/cache/models` | Directory for downloaded model files. |
| `logsDir` | `string` | No | `{appDataDir}/logs` | Directory for log files. |
| `webServiceUrls` | `string` | No | — | Semicolon-separated listener URLs for the web service (e.g. `"http://127.0.0.1:8080"`). |
| `serviceEndpoint` | `string` | No | — | URL of an external Foundry Local service to connect to instead of using the embedded core. |
| `libraryPath` | `string` | No | — | Path to the native library. Normally auto-detected. |
| `additionalSettings` | `Record<string, string>` | No | — | Additional key-value settings passed to the native core. |

---

## Catalog

Discovers and lists AI models available in the system. Accessed via `manager.catalog`.

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `name` | `string` | The name of the catalog. |

### Methods

#### `getModels()`

Lists all available models in the catalog.

**Returns:** `Promise<Model[]>`

```typescript
const models = await manager.catalog.getModels();
for (const model of models) {
  console.log(model.alias, model.variants.length, 'variants');
}
```

#### `getModel(alias)`

Retrieves a model by its alias (e.g. `"qwen2.5-0.5b"`).

| Parameter | Type | Description |
|-----------|------|-------------|
| `alias` | `string` | The alias of the model to retrieve. |

**Returns:** `Promise<Model>`

**Throws:** `Error` if alias is empty or model is not found.

#### `getModelVariant(modelId)`

Retrieves a specific model variant by its unique ID.

| Parameter | Type | Description |
|-----------|------|-------------|
| `modelId` | `string` | The unique identifier of the model variant. |

**Returns:** `Promise<ModelVariant>`

**Throws:** `Error` if modelId is empty or variant is not found.

#### `getCachedModels()`

Returns all model variants that have been downloaded to the local cache.

**Returns:** `Promise<ModelVariant[]>`

#### `getLoadedModels()`

Returns all model variants currently loaded into memory.

**Returns:** `Promise<ModelVariant[]>`

---

## Model

Represents a logical AI model (e.g. "qwen2.5-0.5b") that may have multiple variants. Operations are performed on the currently selected variant. Implements `IModel`.

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `alias` | `string` | The model alias (e.g. `"qwen2.5-0.5b"`). |
| `id` | `string` | The ID of the currently selected variant. |
| `isCached` | `boolean` | Whether the selected variant is downloaded to the local cache. |
| `path` | `string` | Local file path of the selected variant. |
| `variants` | `ModelVariant[]` | All available variants for this model. |

### Methods

#### `selectVariant(modelId)`

Selects a specific variant by its ID.

| Parameter | Type | Description |
|-----------|------|-------------|
| `modelId` | `string` | The ID of the variant to select. |

**Returns:** `void`

**Throws:** `Error` if no variant with the given ID exists.

#### `download(progressCallback?)`

Downloads the selected variant to the local cache.

| Parameter | Type | Description |
|-----------|------|-------------|
| `progressCallback` | `(progress: number) => void` | Optional callback receiving download progress (0–100). |

**Returns:** `Promise<void>`

```typescript
await model.download((progress) => {
  console.log(`Download: ${progress.toFixed(1)}%`);
});
```

#### `load()`

Loads the selected variant into memory for inference.

**Returns:** `Promise<void>`

#### `unload()`

Unloads the selected variant from memory.

**Returns:** `Promise<void>`

#### `isLoaded()`

Checks whether the selected variant is currently loaded in memory.

**Returns:** `Promise<boolean>`

#### `removeFromCache()`

Deletes the selected variant from the local cache.

**Returns:** `void`

#### `createChatClient()`

Creates a ChatClient for performing chat completions with this model.

**Returns:** `ChatClient`

#### `createAudioClient()`

Creates an AudioClient for performing audio transcription with this model.

**Returns:** `AudioClient`

#### `addVariant(variant)`

Adds a variant to this model. Automatically selects it if it is cached and the current one is not.

| Parameter | Type | Description |
|-----------|------|-------------|
| `variant` | `ModelVariant` | The variant to add. Must share the same alias. |

**Returns:** `void`

**Throws:** `Error` if the variant's alias does not match.

---

## ModelVariant

A specific variant of a model (e.g. a particular quantization or device target). Contains detailed model metadata and implements the same lifecycle interface as `Model`. Implements `IModel`.

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `alias` | `string` | The model alias. |
| `id` | `string` | The unique identifier of this variant. |
| `isCached` | `boolean` | Whether this variant is downloaded locally. |
| `path` | `string` | Local file path of this variant. |
| `modelInfo` | `ModelInfo` | Detailed metadata about this variant. |

### Methods

Same lifecycle methods as `Model`: `download()`, `load()`, `unload()`, `isLoaded()`, `removeFromCache()`, `createChatClient()`, `createAudioClient()`.

---

## ModelInfo

Detailed metadata for a model variant. Returned by `ModelVariant.modelInfo`.

| Property | Type | Description |
|----------|------|-------------|
| `id` | `string` | Unique model variant identifier. |
| `name` | `string` | Display name. |
| `alias` | `string` | Model alias. |
| `version` | `number` | Version number. |
| `uri` | `string` | Download URI. |
| `fileSizeBytes` | `number` | File size in bytes. |
| `promptTemplate` | `PromptTemplate` | Template for structuring prompts. |
| `runtime` | `Runtime` | Runtime configuration (device type, execution provider). |
| `isCached` | `boolean` | Whether this variant is downloaded. |
| `modelSettings` | `ModelSettings` | Model-specific parameters. |

---

## ChatClient

Client for performing chat completions with a loaded model. Follows the OpenAI Chat Completions API structure.

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `settings` | `ChatClientSettings` | Configuration settings for chat completions. |

### Methods

#### `completeChat(messages)`

Performs a non-streaming chat completion.

| Parameter | Type | Description |
|-----------|------|-------------|
| `messages` | `any[]` | Array of message objects with `role` and `content` fields. |

**Returns:** `Promise<any>` — The chat completion response.

**Throws:** `Error` if messages are invalid or completion fails.

```typescript
const response = await chat.completeChat([
  { role: 'system', content: 'You are a helpful assistant.' },
  { role: 'user', content: 'What is 2+2?' }
]);
console.log(response.choices[0].message.content);
```

#### `completeStreamingChat(messages, callback)`

Performs a streaming chat completion. The callback receives each response chunk as it arrives.

| Parameter | Type | Description |
|-----------|------|-------------|
| `messages` | `any[]` | Array of message objects. |
| `callback` | `(chunk: any) => void` | Callback invoked for each streaming chunk. |

**Returns:** `Promise<void>` — Resolves when the stream completes.

**Throws:** `Error` if messages or callback are invalid, or streaming fails.

```typescript
await chat.completeStreamingChat(
  [{ role: 'user', content: 'Tell me a story' }],
  (chunk) => {
    const delta = chunk.choices?.[0]?.delta?.content;
    if (delta) process.stdout.write(delta);
  }
);
```

---

## ChatClientSettings

Configuration for chat completion requests. Assign to `chatClient.settings` before calling completion methods.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `temperature` | `number?` | — | Sampling temperature (0–2). Higher values produce more random output. |
| `maxTokens` | `number?` | — | Maximum number of tokens to generate. |
| `topP` | `number?` | — | Nucleus sampling threshold (0–1). |
| `topK` | `number?` | — | Top-K sampling parameter. |
| `frequencyPenalty` | `number?` | — | Penalizes tokens based on frequency in the output so far (-2 to 2). |
| `presencePenalty` | `number?` | — | Penalizes tokens based on presence in the output so far (-2 to 2). |
| `n` | `number?` | — | Number of completions to generate. |
| `randomSeed` | `number?` | — | Random seed for reproducible outputs. |

```typescript
const chat = model.createChatClient();
chat.settings.temperature = 0.7;
chat.settings.maxTokens = 512;
```

---

## AudioClient

Client for performing audio transcription with a loaded model. Follows the OpenAI Audio API structure.

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `settings` | `AudioClientSettings` | Configuration settings for audio operations. |

### Methods

#### `transcribe(audioFilePath)`

Transcribes an audio file.

| Parameter | Type | Description |
|-----------|------|-------------|
| `audioFilePath` | `string` | Path to the audio file (supported format: mp3). |

**Returns:** `Promise<any>` — The transcription result.

**Throws:** `Error` if the file path is invalid or transcription fails.

```typescript
const result = await audio.transcribe('./recording.mp3');
console.log(result.text);
```

#### `transcribeStreaming(audioFilePath, callback)`

Transcribes an audio file with streaming output.

| Parameter | Type | Description |
|-----------|------|-------------|
| `audioFilePath` | `string` | Path to the audio file. |
| `callback` | `(chunk: any) => void` | Callback invoked for each streaming chunk. |

**Returns:** `Promise<void>` — Resolves when the stream completes.

**Throws:** `Error` if the file path or callback is invalid, or streaming fails.

```typescript
await audio.transcribeStreaming('./recording.mp3', (chunk) => {
  if (chunk.text) process.stdout.write(chunk.text);
});
```

---

## AudioClientSettings

Configuration for audio transcription requests. Assign to `audioClient.settings` before calling transcription methods.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `language` | `string?` | — | Language of the audio (ISO 639-1 code, e.g. `"en"`). |
| `temperature` | `number?` | — | Sampling temperature for transcription. |

---

## Enums and Types

### DeviceType

```typescript
enum DeviceType {
  Invalid = 0,
  CPU = 1,
  GPU = 2,
  NPU = 3
}
```

### Runtime

| Property | Type | Description |
|----------|------|-------------|
| `deviceType` | `DeviceType` | The target hardware device. |
| `executionProvider` | `string` | The execution provider name (e.g. `"CUDAExecutionProvider"`). |

### PromptTemplate

| Property | Type | Description |
|----------|------|-------------|
| `system` | `string` | Template for system messages. |
| `user` | `string` | Template for user messages. |
| `assistant` | `string` | Template for assistant messages. |
| `prompt` | `string` | Overall prompt template. |

---

## Complete Example

```typescript
import { FoundryLocalManager } from 'foundry-local-sdk';

async function main() {
  // 1. Initialize
  const manager = FoundryLocalManager.create({
    appName: 'MyApp',
    logLevel: 'info'
  });

  // 2. Discover and select a model
  const model = await manager.catalog.getModel('qwen2.5-0.5b');
  console.log(`Model: ${model.alias}, Variants: ${model.variants.length}`);

  // 3. Download with progress
  await model.download((progress) => {
    process.stdout.write(`\rDownloading: ${progress.toFixed(1)}%`);
  });
  console.log('\nDownload complete.');

  // 4. Load into memory
  await model.load();

  // 5. Chat completion
  const chat = model.createChatClient();
  chat.settings.temperature = 0.7;
  chat.settings.maxTokens = 256;

  // Non-streaming
  const response = await chat.completeChat([
    { role: 'user', content: 'What is Foundry Local?' }
  ]);
  console.log(response.choices[0].message.content);

  // Streaming
  await chat.completeStreamingChat(
    [{ role: 'user', content: 'Tell me a joke' }],
    (chunk) => {
      const text = chunk.choices?.[0]?.delta?.content;
      if (text) process.stdout.write(text);
    }
  );

  // 6. Cleanup
  await model.unload();
}

main().catch(console.error);
```

## Web Service Example

Start an OpenAI-compatible REST endpoint alongside the SDK:

```typescript
import { FoundryLocalManager } from 'foundry-local-sdk';

const manager = FoundryLocalManager.create({
  appName: 'MyApp',
  webServiceUrls: 'http://127.0.0.1:8080'
});

const model = await manager.catalog.getModel('qwen2.5-0.5b');
await model.download();
await model.load();

// Start the web service
manager.startWebService();
console.log('Web service running at:', manager.urls);

// The service exposes OpenAI-compatible endpoints:
//   POST /v1/chat/completions
//   GET  /v1/models
//   GET  /v1/models/{model_id}

// Stop when done
manager.stopWebService();
await model.unload();
```
