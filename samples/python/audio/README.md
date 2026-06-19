# Audio Transcription Example (Live + File)

Transcribe audio two ways with the Foundry Local Python SDK:

- **Live microphone** streaming with **Nemotron ASR** (default).
- **File-based** transcription with **Whisper** via `--file [path]`.

## Prerequisites

- [Foundry Local](https://github.com/microsoft/Foundry-Local) installed
- Python 3.11+
- A microphone (for live mode only — falls back to synthetic audio with
  `--synth` or if PyAudio is unavailable). File mode needs no microphone.

## Setup

This sample installs the Foundry Local SDK **from local repo source** (an
editable install of `sdk/python`), so it always tracks `main` rather than a
published PyPI release:

```bash
cd samples/python/audio
pip install -r requirements.txt
```

That installs:

- `foundry-local-sdk` (editable, from `../../../sdk/python`)

> **`pyaudio` is optional** — it provides cross-platform microphone capture for
> live mode. Without it, live mode falls back to synthetic audio, and `--file`
> transcription works regardless. Install manually if needed:
>
> ```bash
> pip install pyaudio
> ```

## Run

### Live microphone (default — Nemotron ASR)

```bash
python src/app.py
```

Speak into your microphone. Transcription appears in real-time. Press `Ctrl+C`
to stop.

To force synthetic audio (e.g., for CI or when no microphone is available):

```bash
python src/app.py --synth
```

### File-based (Whisper)

Transcribe the bundled `src/Recording.mp3`:

```bash
python src/app.py --file
```

Or transcribe a specific file:

```bash
python src/app.py --file path/to/audio.wav
```

## How it works

**Live mode (Nemotron ASR):**

1. Initializes the SDK and loads the Nemotron streaming ASR model.
2. Creates a `LiveAudioTranscriptionSession` with 16kHz/16-bit/mono PCM settings.
3. Captures microphone audio via `pyaudio` (or generates synthetic audio).
4. Pushes PCM chunks to the SDK via `session.append()`.
5. Reads transcription results in a background thread via
   `for result in session.get_stream()`.

**File mode (Whisper):**

1. Initializes the SDK and loads the `whisper-tiny` model.
2. Calls `audio_client.transcribe(audio_file)` and prints the result text.

## API

```python
# Live streaming
audio_client = model.get_audio_client()
session = audio_client.create_live_transcription_session()
session.settings.sample_rate = 16000
session.settings.channels = 1
session.settings.language = "en"

session.start()
session.append(pcm_bytes)                 # push audio
for result in session.get_stream():       # read results (background thread)
    print(result.content[0].text)         # transcribed text
    print(result.is_final)                # True for final results
session.stop()

# File transcription
audio_client = model.get_audio_client()
result = audio_client.transcribe("Recording.mp3")
print(result.text)
```
