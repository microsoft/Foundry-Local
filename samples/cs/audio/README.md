# Audio Transcription Example

One sample, two transcription modes against Foundry Local:

- **Live microphone streaming** (default) with **Nemotron ASR** — real-time speech-to-text.
- **File-based transcription** with **Whisper** via the `--file [path]` option.

## Prerequisites

- [Foundry Local](https://github.com/microsoft/Foundry-Local) installed
- .NET 9 SDK
- A microphone for live mode (optional — falls back to synthetic audio on non-Windows or with `--synth`)

> **Note:** Microphone capture uses [NAudio](https://github.com/naudio/NAudio) and is Windows-only.
> On other platforms the live mode falls back to synthetic audio; use `--file` for real
> transcription of an audio file.

## SDK consumption

This sample tracks `main`: it consumes the Foundry Local C# SDK **from local source** via a
`ProjectReference` to `sdk/cs/src/Microsoft.AI.Foundry.Local.csproj`. It is **not** version-pinned
to a published package. The `Microsoft.AI.Foundry.Local.Core*` packages and native runtime assets
flow transitively from that project and restore from nuget.org, along with the third-party `NAudio`
and `Microsoft.Extensions.Logging` packages.

## Build & run

```bash
# from this directory
dotnet build
```

`dotnet build` resolves the SDK from `sdk/cs` source via the project reference and restores
`Microsoft.AI.Foundry.Local.Core` plus third-party packages from nuget.org.

### Live microphone transcription (default — Nemotron ASR)

```bash
dotnet run
```

Speak into your microphone. Transcription appears in real-time (cyan text). Press `ENTER` to stop.

To force synthetic audio (e.g., for CI or non-Windows):

```bash
dotnet run -- --synth
```

### File-based transcription (Whisper)

```bash
# transcribe the bundled Recording.mp3
dotnet run -- --file

# transcribe your own file
dotnet run -- --file /path/to/audio.wav
```

A small `Recording.mp3` is bundled and used as the default when no path is given.

## How it works

### Live mode (Nemotron ASR)

1. Loads the Nemotron streaming ASR model.
2. Creates a live transcription session (`audioClient.CreateLiveTranscriptionSession()`) with
   16kHz / 16-bit / mono PCM settings.
3. Captures microphone audio via `NAudio.WaveInEvent` (or generates synthetic audio as a fallback).
4. Pushes PCM chunks via `session.AppendAsync()` through a bounded channel for backpressure.
5. Reads results via `await foreach (var result in session.GetStream())`.

### File mode (Whisper)

1. Loads the `whisper-tiny` model and selects its CPU variant.
2. Streams the transcript via `audioClient.TranscribeAudioStreamingAsync(path)`.

## API

```csharp
// Live streaming
var audioClient = await model.GetAudioClientAsync();
var session = audioClient.CreateLiveTranscriptionSession();
session.Settings.SampleRate = 16000;
session.Settings.Channels = 1;
session.Settings.Language = "en";

await session.StartAsync();
await session.AppendAsync(pcmBytes);          // push audio
await foreach (var result in session.GetStream())
{
    Console.WriteLine(result.Content[0].Text); // transcribed text
    Console.WriteLine(result.IsFinal);         // true for final results
}
await session.StopAsync();

// File transcription
var audioClient = await model.GetAudioClientAsync();
audioClient.Settings.Language = "en";
await foreach (var chunk in audioClient.TranscribeAudioStreamingAsync(path))
{
    Console.Write(chunk.Text);
}
```
