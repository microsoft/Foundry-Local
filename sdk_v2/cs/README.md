# Foundry Local C# SDK

The Foundry Local C# SDK provides a .NET interface for running AI models locally via the Foundry Local Core. Discover, download, load, and run inference entirely on your own machine — no cloud required.

## Features

- **Model catalog** — browse and search all available models; filter by cached or loaded state
- **Lifecycle management** — download, load, unload, and remove models programmatically
- **Chat completions** — synchronous and `IAsyncEnumerable` streaming via OpenAI-compatible types
- **Audio transcription** — transcribe audio files with streaming support
- **Download progress** — wire up an `Action<float>` callback for real-time download percentage
- **Model variants** — select specific hardware/quantization variants per model alias
- **Optional web service** — start an OpenAI-compatible REST endpoint (`/v1/chat_completions`, `/v1/models`)
- **WinML acceleration** — opt-in Windows hardware acceleration with automatic EP download
- **Full async/await** — every operation supports `CancellationToken` and async patterns
- **IDisposable** — deterministic cleanup of native resources

## Installation

```bash
dotnet add package Microsoft.AI.Foundry.Local
```

### Building from source

```bash
cd sdk_v2/cs
dotnet build src/Microsoft.AI.Foundry.Local.csproj
```

Or open [Microsoft.AI.Foundry.Local.SDK.sln](./Microsoft.AI.Foundry.Local.SDK.sln) in Visual Studio / VS Code.

## WinML: Automatic Hardware Acceleration (Windows)

On Windows, Foundry Local can leverage WinML for GPU/NPU hardware acceleration via ONNX Runtime execution providers (EPs). EPs are large binaries downloaded on first use and cached for subsequent runs.

Install the WinML package variant instead:

```bash
dotnet add package Microsoft.AI.Foundry.Local.WinML
```

Or build from source with:

```bash
dotnet build src/Microsoft.AI.Foundry.Local.csproj /p:UseWinML=true
```

### Triggering EP download

EP download can be time-consuming. Call `EnsureEpsDownloadedAsync` early to separate the download step from catalog access:

```csharp
await FoundryLocalManager.Instance.EnsureEpsDownloadedAsync();

// Now catalog access won't trigger an EP download
var catalog = await FoundryLocalManager.Instance.GetCatalogAsync();
```

If you skip this step, EPs are downloaded automatically the first time you access the catalog. Once cached, subsequent calls are fast.

## Quick Start

```csharp
using Microsoft.AI.Foundry.Local;
using Microsoft.Extensions.Logging;
using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;

// 1. Initialize the singleton manager
await FoundryLocalManager.CreateAsync(
    new Configuration { AppName = "my-app" },
    NullLogger.Instance);

// 2. Get the model catalog and look up a model
var catalog = await FoundryLocalManager.Instance.GetCatalogAsync();
var model = await catalog.GetModelAsync("phi-3.5-mini");

// 3. Download (if needed) and load the model
await model!.DownloadAsync();
await model.LoadAsync();

// 4. Get a chat client and run inference
var chatClient = await model.GetChatClientAsync();
var response = await chatClient.CompleteChatAsync(new[]
{
    ChatMessage.FromUser("Why is the sky blue?")
});

Console.WriteLine(response.Choices![0].Message.Content);

// 5. Clean up
FoundryLocalManager.Instance.Dispose();
```

## Usage

### Initialization

`FoundryLocalManager` is an async singleton. Call `CreateAsync` once at startup:

```csharp
await FoundryLocalManager.CreateAsync(
    new Configuration { AppName = "my-app" },
    loggerFactory.CreateLogger("FoundryLocal"));
```

Access it anywhere afterward via `FoundryLocalManager.Instance`. Check `FoundryLocalManager.IsInitialized` to verify creation.

### Catalog

The catalog lists all models known to the Foundry Local Core:

```csharp
var catalog = await FoundryLocalManager.Instance.GetCatalogAsync();

// List all available models
var models = await catalog.ListModelsAsync();
foreach (var m in models)
    Console.WriteLine($"{m.Alias} — {m.SelectedVariant.Info.DisplayName}");

// Get a specific model by alias
var model = await catalog.GetModelAsync("phi-3.5-mini");

// Get a specific variant by its unique model ID
var variant = await catalog.GetModelVariantAsync("phi-3.5-mini-generic-gpu-4");

// List models already downloaded to the local cache
var cached = await catalog.GetCachedModelsAsync();

// List models currently loaded in memory
var loaded = await catalog.GetLoadedModelsAsync();
```

### Model Lifecycle

Each `Model` wraps one or more `ModelVariant` entries (different quantizations, hardware targets). The SDK auto-selects the best variant, or you can pick one:

```csharp
// Check and select variants
Console.WriteLine($"Selected: {model.SelectedVariant.Id}");
foreach (var v in model.Variants)
    Console.WriteLine($"  {v.Id} (cached: {await v.IsCachedAsync()})");

// Switch to a different variant
model.SelectVariant(model.Variants[1]);
```

Download, load, and unload:

```csharp
// Download with progress reporting
await model.DownloadAsync(progress =>
    Console.WriteLine($"Download: {progress:F1}%"));

// Load into memory
await model.LoadAsync();

// Unload when done
await model.UnloadAsync();

// Remove from local cache entirely
await model.RemoveFromCacheAsync();
```

### Chat Completions

```csharp
var chatClient = await model.GetChatClientAsync();

var response = await chatClient.CompleteChatAsync(new[]
{
    ChatMessage.FromSystem("You are a helpful assistant."),
    ChatMessage.FromUser("Explain async/await in C#.")
});

Console.WriteLine(response.Choices![0].Message.Content);
```

#### Streaming

Use `IAsyncEnumerable` for token-by-token output:

```csharp
using var cts = new CancellationTokenSource();

await foreach (var chunk in chatClient.CompleteChatStreamingAsync(
    new[] { ChatMessage.FromUser("Write a haiku about .NET") }, cts.Token))
{
    Console.Write(chunk.Choices?[0]?.Delta?.Content);
}
```

#### Chat Settings

Tune generation parameters per client:

```csharp
chatClient.Settings.Temperature = 0.7f;
chatClient.Settings.MaxTokens = 256;
chatClient.Settings.TopP = 0.9f;
chatClient.Settings.FrequencyPenalty = 0.5f;
```

### Audio Transcription

```csharp
var audioClient = await model.GetAudioClientAsync();

// One-shot transcription
var result = await audioClient.TranscribeAudioAsync("recording.mp3");
Console.WriteLine(result.Text);

// Streaming transcription
await foreach (var chunk in audioClient.TranscribeAudioStreamingAsync("recording.mp3", CancellationToken.None))
{
    Console.Write(chunk.Text);
}
```

#### Audio Settings

```csharp
audioClient.Settings.Language = "en";
audioClient.Settings.Temperature = 0.0f;
```

### Web Service

Start an OpenAI-compatible REST endpoint for use by external tools or processes:

```csharp
// Configure the web service URL in your Configuration
await FoundryLocalManager.CreateAsync(
    new Configuration
    {
        AppName = "my-app",
        Web = new Configuration.WebService { Urls = "http://127.0.0.1:5000" }
    },
    NullLogger.Instance);

await FoundryLocalManager.Instance.StartWebServiceAsync();
Console.WriteLine($"Listening on: {string.Join(", ", FoundryLocalManager.Instance.Urls!)}");

// ... use the service ...

await FoundryLocalManager.Instance.StopWebServiceAsync();
```

### Configuration

| Property | Type | Default | Description |
|---|---|---|---|
| `AppName` | `string` | **(required)** | Your application name |
| `AppDataDir` | `string?` | `~/.{AppName}` | Application data directory |
| `ModelCacheDir` | `string?` | `{AppDataDir}/cache/models` | Where models are stored locally |
| `LogsDir` | `string?` | `{AppDataDir}/logs` | Log output directory |
| `LogLevel` | `LogLevel` | `Warning` | `Verbose`, `Debug`, `Information`, `Warning`, `Error`, `Fatal` |
| `Web` | `WebService?` | `null` | Web service configuration (see below) |
| `AdditionalSettings` | `IDictionary<string, string>?` | `null` | Extra key-value settings passed to Core |

**`Configuration.WebService`**

| Property | Type | Default | Description |
|---|---|---|---|
| `Urls` | `string?` | `127.0.0.1:0` | Bind address; semi-colon separated for multiple |
| `ExternalUrl` | `Uri?` | `null` | URI for accessing the web service in a separate process |

### Disposal

`FoundryLocalManager` implements `IDisposable`. Dispose stops the web service (if running) and releases native resources:

```csharp
FoundryLocalManager.Instance.Dispose();
```

## API Reference

Auto-generated API docs live in [`docs/api/`](./docs/api/). See [`GENERATE-DOCS.md`](./GENERATE-DOCS.md) to regenerate.

Key types:

| Type | Description |
|---|---|
| [`FoundryLocalManager`](./docs/api/microsoft.ai.foundry.local.foundrylocalmanager.md) | Singleton entry point — create, catalog, web service |
| [`Configuration`](./docs/api/microsoft.ai.foundry.local.configuration.md) | Initialization settings |
| [`ICatalog`](./docs/api/microsoft.ai.foundry.local.icatalog.md) | Model catalog interface |
| [`Model`](./docs/api/microsoft.ai.foundry.local.model.md) | Model with variant selection |
| [`ModelVariant`](./docs/api/microsoft.ai.foundry.local.modelvariant.md) | Specific model variant (hardware/quantization) |
| [`OpenAIChatClient`](./docs/api/microsoft.ai.foundry.local.openaichatclient.md) | Chat completions (sync + streaming) |
| [`OpenAIAudioClient`](./docs/api/microsoft.ai.foundry.local.openaiaudioclient.md) | Audio transcription (sync + streaming) |
| [`ModelInfo`](./docs/api/microsoft.ai.foundry.local.modelinfo.md) | Full model metadata record |

## Tests

```bash
dotnet test
```

See [`test/FoundryLocal.Tests/LOCAL_MODEL_TESTING.md`](./test/FoundryLocal.Tests/LOCAL_MODEL_TESTING.md) for prerequisites and local model setup.
