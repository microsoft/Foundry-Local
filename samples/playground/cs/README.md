# Foundry Local Playground — C\#

> [JavaScript](../js/README.md) · [Python](../python/README.md) · **[C#](.)** · [Rust](../rust/README.md)

An interactive CLI sample that demonstrates the full Foundry Local C# SDK — from hardware discovery to streaming inference.

## Prerequisites

- [.NET 9.0 SDK](https://dotnet.microsoft.com/download/dotnet/9.0) or later

## Setup

No explicit install step — `dotnet run` restores packages automatically.

The project uses `Microsoft.AI.Foundry.Local.WinML` `1.0.0-rc2` for Windows hardware acceleration. The `nuget.config` file is configured for the `ORT-Nightly` feed where RC2 packages are published.

> **Cross-platform:** To target macOS/Linux, change the package reference in `FoundryPlayground.csproj`:
> ```xml
> <PackageReference Include="Microsoft.AI.Foundry.Local" Version="1.0.0-rc2" />
> ```
> and update `<TargetFramework>` to `net9.0` (remove the `-windows10.0.26100` suffix).

## Build & Run

For **x64** Windows:

```bash
cd cs
dotnet run -r win-x64
```

For **ARM64** Windows:

```bash
cd cs
dotnet run -r win-arm64
```

## What Happens

1. **Execution providers** are discovered and downloaded with live progress bars.
2. A **model catalog** table is displayed — enter a number to pick a model.
3. The model is **downloaded** (if needed) and **loaded** into memory.
4. Depending on the model type:
   - **Chat models** → interactive conversation with streaming token output in bordered boxes.
   - **Whisper models** → audio transcription — enter a `.wav`/`.mp3` file path and see the transcript stream in.
5. Type `/quit` to exit.

## File Overview

| File | Purpose |
|---|---|
| `Program.cs` | Main SDK flow — initialize, discover EPs, browse catalog, load model, run inference |
| `Ui.cs` | Terminal UI helpers — progress bars, box-drawing tables, streaming output boxes |
| `FoundryPlayground.csproj` | Project file with NuGet dependencies |
| `nuget.config` | NuGet feed configuration (nuget.org + ORT) |

## Key SDK APIs Used

```csharp
using Microsoft.AI.Foundry.Local;
using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;

// Initialize
await FoundryLocalManager.CreateAsync(config, logger);
var manager = FoundryLocalManager.Instance;

// Execution providers
var eps = manager.DiscoverEps();
await manager.DownloadAndRegisterEpsAsync(names, progressCallback);

// Model catalog
var catalog = await manager.GetCatalogAsync();
var models  = await catalog.ListModelsAsync();
var model   = await catalog.GetModelAsync(alias);
model.SelectVariant(variant);

// Download & load
await model.DownloadAsync(progress => { ... });
await model.LoadAsync();

// Chat inference (streaming)
var client = await model.GetChatClientAsync();
await foreach (var chunk in client.CompleteChatStreamingAsync(messages, ct))
{
    Console.Write(chunk.Choices?[0]?.Delta?.Content);
}

// Audio transcription (streaming)
var audio = await model.GetAudioClientAsync();
await foreach (var chunk in audio.TranscribeAudioStreamingAsync(path, ct))
{
    Console.Write(chunk.Text);
}

// Cleanup
await model.UnloadAsync();
manager.Dispose();
```

## Screenshot

```
────────────────────────────────────────────────────────────────────
  Chat  (type a message, /quit to exit)
────────────────────────────────────────────────────────────────────
  What is the capital of France?

  ┌──────────────────────────────────────────────────────────────┐
  │ The capital of France is Paris. It is the largest city in   │
  │ France and serves as the country's political, economic,     │
  │ and cultural center.▍                                       │
  └──────────────────────────────────────────────────────────────┘
```
