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
2. Creates a `LiveAudioTranscriptionClient` with 16kHz/16-bit/mono PCM settings
3. Captures microphone audio via `naudiodon2` (or generates synthetic audio as fallback)
4. Pushes PCM chunks to the SDK via `client.append()`
5. Reads transcription results via `for await (const result of client.getTranscriptionStream())`
6. Access text via `result.content[0].text` (OpenAI Realtime ConversationItem pattern)

## API

```javascript
const audioClient = model.createAudioClient();
const client = audioClient.createLiveTranscriptionClient();
client.settings.sampleRate = 16000;
client.settings.channels = 1;
client.settings.language = 'en';

await client.start();

// Push audio
await client.append(pcmBytes);

// Read results
for await (const result of client.getTranscriptionStream()) {
    console.log(result.content[0].text);       // transcribed text
    console.log(result.content[0].transcript); // alias (OpenAI compat)
    console.log(result.is_final);              // true for final results
}

await client.stop();
```
