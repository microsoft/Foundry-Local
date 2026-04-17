# Live Audio Transcription Example

Real-time microphone-to-text transcription using the Foundry Local JS SDK with Nemotron ASR. Runs entirely on-device — no cloud dependencies.

## Prerequisites

- [Foundry Local](https://github.com/microsoft/Foundry-Local) installed and running (`foundry service status` to verify)
- Node.js 18+
- A microphone (optional — falls back to synthetic audio if unavailable)
- Windows x64 (macOS/Linux support coming soon)

## Setup

### 1. Install dependencies

The SDK and microphone packages are hosted on different registries. Install them in order:

```bash
# Install microphone capture library first (from npmjs)
npm install naudiodon2

# Install Foundry Local JS SDK (from ORT-Nightly feed)
npm install foundry-local-sdk@1.0.0-dev.202604162327 --registry=https://pkgs.dev.azure.com/aiinfra/PublicPackages/_packaging/ORT-Nightly/npm/registry/
```

> **Important:** Install `naudiodon2` before the SDK. Using `npm install` with both packages at once will fail because they are on different registries.

> **Note:** `naudiodon2` requires native compilation tools (Python 3, Visual Studio Build Tools on Windows). If it fails to install, the example will still work using synthetic audio for testing.

### 2. Verify installation

```bash
npm list --depth=0
```

You should see both `foundry-local-sdk` and `naudiodon2` listed.

## Run

```bash
node app.js
```

### What to expect

1. The SDK initializes and connects to Foundry Local
2. The Nemotron ASR model is downloaded on first run (~700 MB, cached for future runs)
3. The model loads into memory
4. Your microphone is detected and audio capture begins
5. Speak naturally — transcription appears in real-time
6. Press `Ctrl+C` to stop

### Example output

```
╔══════════════════════════════════════════════════════════╗
║   Foundry Local — Live Audio Transcription (JS SDK)     ║
╚══════════════════════════════════════════════════════════╝

Initializing Foundry Local SDK...
✓ SDK initialized
Found model: nemotron-speech-streaming-en-0.6b-generic-cpu:1
Downloading model (if needed)...
✓ Model downloaded
Loading model...
✓ Model loaded
Starting streaming session...
✓ Session started

════════════════════════════════════════════════════════════
  LIVE TRANSCRIPTION ACTIVE
  Speak into your microphone.
  Press Ctrl+C to stop.
════════════════════════════════════════════════════════════

 Hey, how are you? Hello, can you hear me?
  [FINAL] Yes, I can hear you clearly.
```

## Troubleshooting

| Issue | Solution |
|-------|----------|
| `Model not found in catalog` | Ensure Foundry Local service is running: `foundry service start` |
| `naudiodon2` install fails | Install Visual Studio Build Tools. Or skip it — the app falls back to synthetic audio. |
| `EPERM` error on `npm install` | Close any running `node` processes, delete `node_modules`, and reinstall |
| Audio overflow warnings | Normal on CPU — the ASR model may briefly fall behind real-time audio |
| OGA leak warnings on exit | Cosmetic only — happens when `Ctrl+C` interrupts before graceful cleanup |

## How it works

1. Initializes the Foundry Local SDK and loads the Nemotron ASR model
2. Creates a `LiveAudioTranscriptionSession` with 16kHz/16-bit/mono PCM settings
3. Captures microphone audio via `naudiodon2` (or generates synthetic audio as fallback)
4. Queues PCM chunks and pumps them to the SDK via `session.append()`
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

await session.stop();
```
