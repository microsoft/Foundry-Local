# Live Audio Transcription Example

Real-time microphone-to-text transcription using the Foundry Local JS SDK with Nemotron ASR.

## Prerequisites

- [Foundry Local](https://github.com/microsoft/Foundry-Local) installed
- Node.js 18+
- A microphone (optional — falls back to synthetic audio)

## Setup

```bash
npm install foundry-local-sdk naudiodon2
```

> **Note:** `naudiodon2` is optional — provides cross-platform microphone capture. Without it, the example falls back to synthetic audio for testing.

## Run

```bash
node app.js
```

Speak into your microphone. Transcription appears in real-time. Press `Ctrl+C` to stop.

## How it works

1. Initializes the Foundry Local SDK and loads the Nemotron ASR model
2. Creates a `LiveAudioTranscriptionSession` with 16kHz/16-bit/mono PCM settings
3. Captures microphone audio via `naudiodon2` (or generates synthetic audio as fallback)
4. Pushes PCM chunks to the SDK via `session.append()`
5. Reads transcription results via `for await (const result of session.getTranscriptionStream())`
6. Access text via `result.content[0].text` (OpenAI Realtime ConversationItem pattern)

## API

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
for await (const result of session.getTranscriptionStream()) {
    console.log(result.content[0].text);       // transcribed text
    console.log(result.content[0].transcript); // alias (OpenAI compat)
    console.log(result.is_final);              // true for final results
}

await client.stop();
```
