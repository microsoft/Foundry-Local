---
title: "C# / .NET API Reference"
description: Complete API reference for the Foundry Local C# SDK. Covers FoundryLocalManager, ICatalog, Model, OpenAIChatClient, OpenAIAudioClient, and configuration.
---

# C# / .NET API Reference

Package: `Microsoft.AI.Foundry.Local`  
Target Framework: .NET 8.0+

```bash
dotnet add package Microsoft.AI.Foundry.Local
```

For WinML hardware acceleration on Windows:

```bash
dotnet add package Microsoft.AI.Foundry.Local.WinML
```

Namespace for all types:

```csharp
using Microsoft.AI.Foundry.Local;
```

---

## FoundryLocalManager

The singleton entry point for the Foundry Local SDK. Manages initialization, provides access to the model catalog, and controls the optional web service. Implements `IDisposable`.

```csharp
public class FoundryLocalManager : IDisposable
```

### Static Properties

| Property | Type | Description |
|----------|------|-------------|
| `IsInitialized` | `bool` | Whether the singleton has been created. |
| `Instance` | `FoundryLocalManager` | The singleton instance. Only valid after `CreateAsync` completes. |
| `Urls` | `string[]?` | Bound URLs after calling `StartWebServiceAsync`. Null if web service is not running. |

### Static Methods

#### `CreateAsync(configuration, logger, ct?)`

Creates the singleton `FoundryLocalManager` instance. Must be called before accessing `Instance`.

| Parameter | Type | Description |
|-----------|------|-------------|
| `configuration` | `Configuration` | Configuration settings. `AppName` is required. |
| `logger` | `ILogger` | Application logger. Use `NullLogger.Instance` to suppress SDK log output. |
| `ct` | `CancellationToken?` | Optional cancellation token. |

**Returns:** `Task`

**Throws:** `FoundryLocalException` if initialization fails.

```csharp
var config = new Configuration { AppName = "MyApp", LogLevel = LogLevel.Information };
await FoundryLocalManager.CreateAsync(config, logger);
var manager = FoundryLocalManager.Instance;
```

### Instance Methods

#### `GetCatalogAsync(ct?)`

Gets the model catalog instance. The catalog is populated on first use.

| Parameter | Type | Description |
|-----------|------|-------------|
| `ct` | `CancellationToken?` | Optional cancellation token. |

**Returns:** `Task<ICatalog>`

> **Note:** If using a WinML build, this call may trigger a one-off execution provider download. Call `EnsureEpsDownloadedAsync` first to separate the two operations.

#### `StartWebServiceAsync(ct?)`

Starts the optional OpenAI-compatible web service. After this call, `Urls` is populated with the bound addresses. The web service exposes:

- `POST /v1/chat_completions` — Chat completions
- `GET /v1/models` — List downloaded models
- `GET /v1/models/{model_id}` — Get model details

| Parameter | Type | Description |
|-----------|------|-------------|
| `ct` | `CancellationToken?` | Optional cancellation token. |

**Returns:** `Task`

#### `StopWebServiceAsync(ct?)`

Stops the web service if it is running.

| Parameter | Type | Description |
|-----------|------|-------------|
| `ct` | `CancellationToken?` | Optional cancellation token. |

**Returns:** `Task`

#### `EnsureEpsDownloadedAsync(ct?)`

Downloads and registers execution providers. Only relevant when using the WinML package. Once downloaded, execution providers are cached and subsequent calls are fast.

| Parameter | Type | Description |
|-----------|------|-------------|
| `ct` | `CancellationToken?` | Optional cancellation token. |

**Returns:** `Task`

#### `Dispose()`

Releases all resources held by the manager.

---

## Configuration

Settings passed to `FoundryLocalManager.CreateAsync()`.

```csharp
public class Configuration
```

| Property | Type | Required | Default | Description |
|----------|------|----------|---------|-------------|
| `AppName` | `string` | **Yes** | — | Your application name. Used to derive default paths. |
| `AppDataDir` | `string?` | No | `~/.{AppName}` | Application data directory. |
| `ModelCacheDir` | `string?` | No | `{AppDataDir}/cache/models` | Directory for downloaded model files. |
| `LogsDir` | `string?` | No | `{AppDataDir}/logs` | Directory for log files. |
| `LogLevel` | `LogLevel` | No | `LogLevel.Warning` | Logging level. |
| `Web` | `WebService?` | No | — | Web service configuration. |
| `AdditionalSettings` | `IDictionary<string, string>?` | No | — | Additional key-value settings for the native core. |

### WebService

Optional configuration for the built-in web service.

| Property | Type | Description |
|----------|------|-------------|
| `Urls` | `string?` | Semicolon-separated listener URLs (e.g. `"http://127.0.0.1:8080"`). |
| `ExternalUrl` | `Uri?` | URL of an external Foundry Local service to connect to. |

```csharp
var config = new Configuration
{
    AppName = "MyApp",
    LogLevel = LogLevel.Information,
    Web = new WebService { Urls = "http://127.0.0.1:8080" }
};
```

### LogLevel

```csharp
public enum LogLevel
{
    Verbose,
    Debug,
    Information,
    Warning,
    Error,
    Fatal
}
```

---

## ICatalog

Interface for discovering and managing models. Obtained via `manager.GetCatalogAsync()`.

```csharp
public interface ICatalog
```

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `Name` | `string` | The catalog name. |

### Methods

#### `ListModelsAsync(ct?)`

Lists all available models in the catalog.

| Parameter | Type | Description |
|-----------|------|-------------|
| `ct` | `CancellationToken?` | Optional cancellation token. |

**Returns:** `Task<List<Model>>`

```csharp
var catalog = await manager.GetCatalogAsync();
var models = await catalog.ListModelsAsync();
foreach (var model in models)
{
    Console.WriteLine($"{model.Alias} ({model.Variants.Count} variants)");
}
```

#### `GetModelAsync(modelAlias, ct?)`

Looks up a model by its alias (e.g. `"qwen2.5-0.5b"`).

| Parameter | Type | Description |
|-----------|------|-------------|
| `modelAlias` | `string` | The model alias to look up. |
| `ct` | `CancellationToken?` | Optional cancellation token. |

**Returns:** `Task<Model?>` — The matching model, or `null` if not found.

#### `GetModelVariantAsync(modelId, ct?)`

Looks up a specific model variant by its unique ID.

| Parameter | Type | Description |
|-----------|------|-------------|
| `modelId` | `string` | The unique model variant identifier. |
| `ct` | `CancellationToken?` | Optional cancellation token. |

**Returns:** `Task<ModelVariant?>` — The matching variant, or `null` if not found.

#### `GetCachedModelsAsync(ct?)`

Returns all model variants that have been downloaded to the local cache.

| Parameter | Type | Description |
|-----------|------|-------------|
| `ct` | `CancellationToken?` | Optional cancellation token. |

**Returns:** `Task<List<ModelVariant>>`

#### `GetLoadedModelsAsync(ct?)`

Returns all model variants currently loaded into memory.

| Parameter | Type | Description |
|-----------|------|-------------|
| `ct` | `CancellationToken?` | Optional cancellation token. |

**Returns:** `Task<List<ModelVariant>>`

---

## Model

Represents a logical AI model with one or more variants. Operations are performed on the currently selected variant. Implements `IModel`.

```csharp
public class Model : IModel
```

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `Alias` | `string` | The model alias (e.g. `"qwen2.5-0.5b"`). |
| `Id` | `string` | The ID of the currently selected variant. |
| `Variants` | `List<ModelVariant>` | All available variants for this model. |
| `SelectedVariant` | `ModelVariant` | The currently selected variant. |

### Methods

#### `SelectVariant(variant)`

Selects a specific variant for subsequent operations.

| Parameter | Type | Description |
|-----------|------|-------------|
| `variant` | `ModelVariant` | The variant to select. Must be one of `Variants`. |

**Returns:** `void`

**Throws:** `FoundryLocalException` if the variant is not valid for this model.

```csharp
// Select a specific variant (e.g. CPU variant)
var cpuVariant = model.Variants.First(v => v.Info.Runtime.DeviceType == DeviceType.CPU);
model.SelectVariant(cpuVariant);
```

#### `GetLatestVersion(variant)`

Returns the latest version of the specified variant.

| Parameter | Type | Description |
|-----------|------|-------------|
| `variant` | `ModelVariant` | The variant to check. |

**Returns:** `ModelVariant` — The latest version. Same as input if already latest.

**Throws:** `FoundryLocalException` if the variant is not valid for this model.

#### `DownloadAsync(downloadProgress?, ct?)`

Downloads the selected variant to the local cache.

| Parameter | Type | Description |
|-----------|------|-------------|
| `downloadProgress` | `Action<float>?` | Optional callback receiving progress (0–100). |
| `ct` | `CancellationToken?` | Optional cancellation token. |

**Returns:** `Task`

```csharp
await model.DownloadAsync(
    progress => Console.Write($"\rDownloading: {progress:F1}%"),
    cancellationToken);
```

#### `LoadAsync(ct?)`

Loads the selected variant into memory for inference.

| Parameter | Type | Description |
|-----------|------|-------------|
| `ct` | `CancellationToken?` | Optional cancellation token. |

**Returns:** `Task`

#### `UnloadAsync(ct?)`

Unloads the selected variant from memory.

| Parameter | Type | Description |
|-----------|------|-------------|
| `ct` | `CancellationToken?` | Optional cancellation token. |

**Returns:** `Task`

#### `IsCachedAsync(ct?)`

Checks whether the selected variant is downloaded to the local cache.

**Returns:** `Task<bool>`

#### `IsLoadedAsync(ct?)`

Checks whether the selected variant is loaded in memory.

**Returns:** `Task<bool>`

#### `GetPathAsync(ct?)`

Gets the local file path of the selected variant.

**Returns:** `Task<string>`

#### `RemoveFromCacheAsync(ct?)`

Deletes the selected variant from the local cache.

**Returns:** `Task`

#### `GetChatClientAsync(ct?)`

Creates an OpenAIChatClient for performing chat completions with this model.

**Returns:** `Task<OpenAIChatClient>`

#### `GetAudioClientAsync(ct?)`

Creates an OpenAIAudioClient for performing audio transcription with this model.

**Returns:** `Task<OpenAIAudioClient>`

---

## ModelVariant

A specific variant of a model (e.g. a particular quantization or device target). Contains detailed metadata and implements the same lifecycle interface as `Model`. Implements `IModel`.

```csharp
public class ModelVariant : IModel
```

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `Alias` | `string` | The model alias. |
| `Id` | `string` | Unique identifier of this variant. |
| `Version` | `int` | Version number. |
| `Info` | `ModelInfo` | Detailed metadata about this variant. |

### Methods

Same lifecycle methods as `Model`: `DownloadAsync()`, `LoadAsync()`, `UnloadAsync()`, `IsCachedAsync()`, `IsLoadedAsync()`, `GetPathAsync()`, `RemoveFromCacheAsync()`, `GetChatClientAsync()`, `GetAudioClientAsync()`.

---

## ModelInfo

Detailed metadata for a model variant. Accessed via `ModelVariant.Info`.

| Property | Type | Description |
|----------|------|-------------|
| `Id` | `string` | Unique model variant identifier. |
| `Name` | `string` | Display name. |
| `Alias` | `string` | Model alias. |
| `Version` | `int` | Version number. |
| `Uri` | `string` | Download URI. |
| `FileSizeBytes` | `long` | File size in bytes. |
| `PromptTemplate` | `PromptTemplate` | Template for structuring prompts. |
| `Runtime` | `Runtime` | Runtime configuration (device type, execution provider). |
| `IsCached` | `bool` | Whether this variant is downloaded. |
| `ModelSettings` | `ModelSettings` | Model-specific parameters. |

---

## OpenAIChatClient

Client for performing chat completions. Uses OpenAI-compatible request/response types (from the Betalgo.Ranul.OpenAI SDK).

```csharp
public class OpenAIChatClient
```

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `Settings` | `ChatSettings` | Configuration settings for chat completions. |

### ChatSettings

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `Temperature` | `float?` | — | Sampling temperature (0–2). |
| `MaxTokens` | `int?` | — | Maximum tokens to generate. |
| `TopP` | `float?` | — | Nucleus sampling threshold (0–1). |
| `FrequencyPenalty` | `float?` | — | Frequency penalty (-2 to 2). |
| `PresencePenalty` | `float?` | — | Presence penalty (-2 to 2). |

### Methods

#### `CompleteChatAsync(messages, ct?)`

Executes a non-streaming chat completion.

| Parameter | Type | Description |
|-----------|------|-------------|
| `messages` | `IEnumerable<ChatMessage>` | Chat messages. The system message is automatically added. |
| `ct` | `CancellationToken?` | Optional cancellation token. |

**Returns:** `Task<ChatCompletionCreateResponse>`

```csharp
var chat = await model.GetChatClientAsync();
chat.Settings.Temperature = 0.7f;
chat.Settings.MaxTokens = 256;

var response = await chat.CompleteChatAsync(new[]
{
    ChatMessage.FromSystem("You are a helpful assistant."),
    ChatMessage.FromUser("What is 2+2?")
});
Console.WriteLine(response.Choices.First().Message.Content);
```

#### `CompleteChatStreamingAsync(messages, ct)`

Executes a streaming chat completion. Returns an `IAsyncEnumerable` so you can process tokens as they arrive.

| Parameter | Type | Description |
|-----------|------|-------------|
| `messages` | `IEnumerable<ChatMessage>` | Chat messages. |
| `ct` | `CancellationToken` | Cancellation token. |

**Returns:** `IAsyncEnumerable<ChatCompletionCreateResponse>`

```csharp
await foreach (var chunk in chat.CompleteChatStreamingAsync(
    new[] { ChatMessage.FromUser("Tell me a story") },
    cancellationToken))
{
    var text = chunk.Choices?.FirstOrDefault()?.Delta?.Content;
    if (text != null) Console.Write(text);
}
```

---

## OpenAIAudioClient

Client for performing audio transcription. Uses OpenAI-compatible request/response types.

```csharp
public class OpenAIAudioClient
```

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `Settings` | `AudioSettings` | Configuration settings for audio transcription. |

### AudioSettings

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `Language` | `string?` | — | Language of the audio (ISO 639-1 code). |
| `Temperature` | `float?` | — | Sampling temperature. |

### Methods

#### `TranscribeAudioAsync(audioFilePath, ct?)`

Transcribes an audio file.

| Parameter | Type | Description |
|-----------|------|-------------|
| `audioFilePath` | `string` | Path to the audio file. Supported format: mp3. |
| `ct` | `CancellationToken?` | Optional cancellation token. |

**Returns:** `Task<AudioCreateTranscriptionResponse>`

```csharp
var audio = await model.GetAudioClientAsync();
var result = await audio.TranscribeAudioAsync("recording.mp3");
Console.WriteLine(result.Text);
```

#### `TranscribeAudioStreamingAsync(audioFilePath, ct)`

Transcribes an audio file with streaming output.

| Parameter | Type | Description |
|-----------|------|-------------|
| `audioFilePath` | `string` | Path to the audio file. Supported format: mp3. |
| `ct` | `CancellationToken` | Cancellation token. |

**Returns:** `IAsyncEnumerable<AudioCreateTranscriptionResponse>`

```csharp
await foreach (var chunk in audio.TranscribeAudioStreamingAsync("recording.mp3", ct))
{
    if (chunk.Text != null) Console.Write(chunk.Text);
}
```

---

## Enums and Types

### DeviceType

```csharp
public enum DeviceType
{
    Invalid = 0,
    CPU = 1,
    GPU = 2,
    NPU = 3
}
```

### Runtime

| Property | Type | Description |
|----------|------|-------------|
| `DeviceType` | `DeviceType` | The target hardware device. |
| `ExecutionProvider` | `string` | The execution provider name (e.g. `"CUDAExecutionProvider"`). |

### PromptTemplate

| Property | Type | Description |
|----------|------|-------------|
| `System` | `string` | Template for system messages. |
| `User` | `string` | Template for user messages. |
| `Assistant` | `string` | Template for assistant messages. |
| `Prompt` | `string` | Overall prompt template. |

### FoundryLocalException

```csharp
public class FoundryLocalException : Exception
```

Base exception type for all errors thrown by the SDK. Includes logging context.

---

## Complete Example

```csharp
using Microsoft.AI.Foundry.Local;
using Microsoft.Extensions.Logging;

// 1. Initialize
var logger = LoggerFactory.Create(b => b.AddConsole()).CreateLogger("MyApp");
var config = new Configuration
{
    AppName = "MyApp",
    LogLevel = LogLevel.Information
};

await FoundryLocalManager.CreateAsync(config, logger);
var manager = FoundryLocalManager.Instance;

// Optional: download execution providers (WinML only)
await manager.EnsureEpsDownloadedAsync();

// 2. Discover and select a model
var catalog = await manager.GetCatalogAsync();
var model = await catalog.GetModelAsync("qwen2.5-0.5b");

// 3. Download with progress
await model.DownloadAsync(progress =>
    Console.Write($"\rDownloading: {progress:F1}%"));
Console.WriteLine("\nDownload complete.");

// 4. Load into memory
await model.LoadAsync();

// 5. Chat completion
var chat = await model.GetChatClientAsync();
chat.Settings.Temperature = 0.7f;
chat.Settings.MaxTokens = 256;

// Non-streaming
var response = await chat.CompleteChatAsync(new[]
{
    ChatMessage.FromUser("What is Foundry Local?")
});
Console.WriteLine(response.Choices.First().Message.Content);

// Streaming
await foreach (var chunk in chat.CompleteChatStreamingAsync(
    new[] { ChatMessage.FromUser("Tell me a joke") },
    CancellationToken.None))
{
    var text = chunk.Choices?.FirstOrDefault()?.Delta?.Content;
    if (text != null) Console.Write(text);
}

// 6. Cleanup
await model.UnloadAsync();
manager.Dispose();
```

## Web Service Example

Start an OpenAI-compatible REST endpoint alongside the SDK:

```csharp
var config = new Configuration
{
    AppName = "MyApp",
    Web = new WebService { Urls = "http://127.0.0.1:8080" }
};

await FoundryLocalManager.CreateAsync(config, logger);
var manager = FoundryLocalManager.Instance;

var catalog = await manager.GetCatalogAsync();
var model = await catalog.GetModelAsync("qwen2.5-0.5b");
await model.DownloadAsync();
await model.LoadAsync();

// Start the web service
await manager.StartWebServiceAsync();
Console.WriteLine($"Web service running at: {string.Join(", ", manager.Urls!)}");

// The service exposes OpenAI-compatible endpoints:
//   POST /v1/chat_completions
//   GET  /v1/models
//   GET  /v1/models/{model_id}

// Stop when done
await manager.StopWebServiceAsync();
await model.UnloadAsync();
manager.Dispose();
```
