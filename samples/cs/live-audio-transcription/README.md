# Live Audio Transcription Example

Real-time microphone-to-text transcription using the Foundry Local C# SDK with Nemotron ASR.

## Prerequisites

- [Foundry Local](https://github.com/microsoft/Foundry-Local) installed
- .NET 9 SDK
- A microphone (optional — falls back to synthetic audio on non-Windows or with `--synth`)

## Setup

```bash
dotnet restore
```

> **Note:** Microphone capture uses [NAudio](https://github.com/naudio/NAudio) and is Windows-only. On other platforms, the sample falls back to synthetic audio for testing.

## Run

```bash
dotnet run
```

Speak into your microphone. Transcription appears in real-time (cyan text). Press `ENTER` to stop recording.

To force synthetic audio (e.g., for CI or non-Windows):

```bash
dotnet run -- --synth
```

## How it works

1. Initializes the Foundry Local SDK and loads the Nemotron ASR model
2. Creates a `LiveAudioTranscriptionSession` with 16kHz/16-bit/mono PCM settings
3. Captures microphone audio via `NAudio.WaveInEvent` (or generates synthetic audio as fallback)
4. Pushes PCM chunks to the SDK via `session.AppendAsync()` through a bounded channel for backpressure
5. Reads transcription results via `await foreach (var result in session.GetTranscriptionStream())`
6. Access text via `result.Content[0].Text` (OpenAI Realtime ConversationItem pattern)

## API

```csharp
var audioClient = await model.GetAudioClientAsync();
var session = audioClient.CreateLiveTranscriptionSession();
session.Settings.SampleRate = 16000;
session.Settings.Channels = 1;
session.Settings.Language = "en";

await session.StartAsync();

// Push audio
await session.AppendAsync(pcmBytes);

// Read results
await foreach (var result in session.GetTranscriptionStream())
{
    Console.WriteLine(result.Content[0].Text);       // transcribed text
    Console.WriteLine(result.Content[0].Transcript); // alias (OpenAI compat)
    Console.WriteLine(result.IsFinal);               // true for final results
}

await session.StopAsync();
```
