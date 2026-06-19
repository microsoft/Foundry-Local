# Audio Transcription

A single app with **two transcription modes** using the Foundry Local JS SDK:

- **Live microphone streaming (default)** — real-time mic-to-text using the **Nemotron** streaming ASR model.
- **File-based transcription** — transcribe an audio file via `--file <path>` using the **Whisper** model. A bundled `Recording.mp3` is used when no path is supplied.

## Prerequisites

- [Foundry Local](https://github.com/microsoft/Foundry-Local) installed
- [Node.js](https://nodejs.org/) v18 or later
- A microphone (live mode only — falls back to synthetic audio if `naudiodon2` is unavailable)

## Install

This sample consumes the JS SDK **directly from local source** (`sdk/js`) so it always tracks
`main` rather than a published npm version. It is **not** pinned to a registry release. The
dependency in `package.json` is:

```json
"foundry-local-sdk": "file:../../../sdk/js"
```

Install dependencies:

```bash
npm install
```

> **Building the SDK:** `npm install` resolves `foundry-local-sdk` from `sdk/js`. The SDK ships a
> prebuilt `dist/` and downloads its native runtime on install. If the local SDK has not been built
> (or you've changed its source), build it first:
>
> ```bash
> cd ../../../sdk/js
> npm install
> npm run build          # compile TypeScript -> dist/
> npm run build:native   # (re)build the native addon if needed
> ```

> **Note:** `naudiodon2` is an **optional** dependency that provides cross-platform microphone
> capture for live mode. Without it, live mode falls back to synthetic audio for testing. File
> mode does not require it.

## Run

```bash
# Live microphone streaming (Nemotron) — default
npm start
# or
node app.js

# Transcribe the bundled Recording.mp3 (Whisper)
node app.js --file

# Transcribe a specific audio file (Whisper)
node app.js --file ./my-audio.mp3
```

In live mode, speak into your microphone — transcription appears in real-time. Press `Ctrl+C` to stop.

## How it works

### Live mode (Nemotron streaming ASR)

1. Loads the Nemotron streaming ASR model and creates a `LiveAudioTranscriptionSession`
   (16kHz / 16-bit / mono PCM).
2. Captures microphone audio via `naudiodon2` (or generates synthetic audio as a fallback).
3. Pushes PCM chunks to the SDK via `session.append()`.
4. Reads results via `for await (const result of session.getStream())`.
5. Accesses text via `result.content[0].text` (OpenAI Realtime ConversationItem pattern).

### File mode (Whisper)

1. Downloads and registers execution providers, then loads the `whisper-tiny` model.
2. Creates an audio client.
3. Runs non-streaming transcription via `audioClient.transcribe(file)`.
4. Runs streaming transcription via `for await (const result of audioClient.transcribeStreaming(file))`.

## API (live mode)

```javascript
const audioClient = model.createAudioClient();
const session = audioClient.createLiveTranscriptionSession();
session.settings.sampleRate = 16000;
session.settings.channels = 1;
session.settings.language = 'en';

await session.start();

// Push audio
await session.append(pcmBytes);

// Read results
for await (const result of session.getStream()) {
    console.log(result.content[0].text);       // transcribed text
    console.log(result.content[0].transcript); // alias (OpenAI compat)
    console.log(result.is_final);              // true for final results
}

await session.stop();
```
