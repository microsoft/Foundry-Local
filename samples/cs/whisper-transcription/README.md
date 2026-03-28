# Whisper Transcription — Foundry Local

An on-device audio transcription web application powered by [Foundry Local](https://foundrylocal.ai) and OpenAI Whisper models. All processing runs locally — no audio data leaves your machine.

Based on the [FLWhisper](https://github.com/leestott/FLWhisper) project.

## Features

- **100% local processing** — audio never leaves your device
- **Streaming transcription** — uses the Foundry Local SDK streaming API for real-time output
- **Web UI** — drag-and-drop or file picker with audio preview
- **REST API** — OpenAI-compatible `/v1/audio/transcriptions` endpoint
- **Health checks** — built-in health endpoint for monitoring
- **Cache-aware** — skips download when the model is already cached

## Prerequisites

- **Windows 10/11** (ARM64 or x64)
- **.NET 9 SDK** — [Download here](https://dotnet.microsoft.com/download/dotnet/9.0)
- **Windows 11 SDK 10.0.26100.0 or newer** — required by the `Microsoft.AI.Foundry.Local.WinML` package used by this sample
- **Foundry Local** — installed and on PATH

## Quick Start

```bash
cd samples/cs/whisper-transcription

# Restore and run
dotnet restore
dotnet run
```

Open **http://localhost:5000** (or the port shown in console output).

On first launch, Foundry Local will download the Whisper model if it is not already cached. Subsequent launches will be near-instant.

## Security Notes

- This sample binds to `localhost` and `127.0.0.1` by default so the unauthenticated upload endpoint stays local-only.
- The transcription POST endpoint keeps `.DisableAntiforgery()` to remain compatible with direct API clients and the sample's browser upload flow.
- If you change the app to listen on LAN or public interfaces, add authentication and re-enable antiforgery protection before exposing it to other machines.

## Project Structure

```
whisper-transcription/
├── Program.cs                      # ASP.NET Core Minimal API entry point
├── Health/
│   └── FoundryHealthCheck.cs       # Health check implementation
├── Middleware/
│   └── ErrorHandlingMiddleware.cs  # Global error handler
├── Services/
│   ├── FoundryOptions.cs           # Configuration options
│   ├── FoundryModelService.cs      # Model management (cache check, download, load)
│   └── TranscriptionService.cs     # Audio transcription via streaming API
├── wwwroot/
│   ├── index.html                  # Web UI
│   ├── app.js                      # Client-side logic
│   └── styles.css                  # Styling
├── appsettings.json                # Configuration
├── nuget.config                    # NuGet package sources
├── WhisperTranscription.csproj     # Project file
└── README.md
```

## API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/health` | ASP.NET Core health check |
| GET | `/api/health/status` | Model status with cache info |
| POST | `/v1/audio/transcriptions` | Transcribe audio (OpenAI compatible) |
| GET | `/swagger` | Interactive API docs (dev mode) |

### Transcription Request

```
POST /v1/audio/transcriptions
Content-Type: multipart/form-data
```

Parameters:
- `file` (required) — audio file (WAV, MP3, M4A, etc.)
- `model` (optional) — model alias (default: from config)
- `format` (optional) — `text` (default) or `json`

## Configuration

Edit `appsettings.json`:

```json
{
  "Foundry": {
    "ModelAlias": "whisper-tiny",
    "LogLevel": "Information"
  }
}
```

Override via environment variable: `Foundry__ModelAlias=whisper-medium`

To override the default local-only binding, set `ASPNETCORE_URLS`, for example `http://127.0.0.1:5001`.

## How It Works

1. **Bootstrap** — `FoundryModelService` initializes the Foundry Local runtime and registers execution providers.
2. **Model resolution** — the configured model alias is resolved from the catalog.
3. **Cache check** — `IsCachedAsync()` skips download when the model is already on disk.
4. **Download** — if not cached, the model is downloaded with progress logging.
5. **Load** — the CPU variant is selected and loaded into the inference engine.
6. **Transcription** — audio is transcribed using `TranscribeAudioStreamingAsync()` for streaming output.
7. **Response** — the full transcription text is returned as plain text or JSON.

## Related Samples

- [AudioTranscriptionExample](../GettingStarted/src/AudioTranscriptionExample/) — console-based Whisper transcription
- [FLWhisper](https://github.com/leestott/FLWhisper) — full-featured medical transcription app

## License

This sample is provided under the [MIT License](../../../LICENSE).
