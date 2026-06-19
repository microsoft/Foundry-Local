# Audio Transcription (C++)

Transcribes audio with Foundry Local using two model paths — matching the
other-language `audio` samples:

- **Live microphone → Nemotron streaming ASR** (`nemotron-speech-streaming-en-0.6b`):
  incremental, real-time transcription, the same flow as
  `sdk_v2/cpp/examples/realtime_audio`.
- **File → Whisper** (`whisper-tiny`): whole-file, non-streaming transcription.

This sample tracks **`main`** — it builds against your **local** `sdk_v2/cpp` build,
not a pinned SDK release.

## What it does

- **Live path** uses a streaming `AudioSession`: a `Request` carries a `pcm` format
  descriptor plus an `ItemQueue`, a background producer pushes PCM chunks into the
  queue, and a streaming callback prints transcribed text as it arrives.
- **File path** uses a non-streaming `AudioSession`: a single `Item::AudioFromUri(path)`
  drives Whisper, and the transcript is read from the response's text item. The SDK
  reads and decodes the file, so no manual PCM handling is needed.

### Modes

| Invocation            | Model                | Source                                          |
|-----------------------|----------------------|-------------------------------------------------|
| *(default)*           | Nemotron (streaming) | Live microphone via PortAudio; falls back to Whisper transcription of the bundled WAV. |
| `--file [path]`       | Whisper              | An audio file. With no path, uses the bundled `Recording.wav`. |
| `--synth`             | Nemotron (streaming) | A generated 440 Hz sine tone (no mic, no file). |

The Nemotron streaming model expects **16 kHz mono** PCM; `Recording.wav` already
matches.

### Live microphone is optional

Live capture uses [PortAudio](http://www.portaudio.com/) and is enabled **only when
PortAudio is found at configure time** (the build defines `HAS_PORTAUDIO` and links
it). Without PortAudio the sample still builds and runs — the default mode falls back
to Whisper transcription of the bundled WAV, and `--file` / `--synth` work as usual.

Install PortAudio for live mic capture:

```bash
# macOS
brew install portaudio
# Debian/Ubuntu
sudo apt-get install portaudio19-dev
```

## Prerequisites

```bash
python ../../../sdk_v2/cpp/build.py
```

## Build

```bash
cmake -S . -B build
cmake --build build
```

Override the SDK config/location if needed:
`-DFOUNDRY_LOCAL_BUILD_CONFIG=Debug`, `-DFOUNDRY_LOCAL_SDK_DIR=...`,
`-DFOUNDRY_LOCAL_BUILD_DIR=...`.

## Run

```bash
./build/audio              # live mic (Nemotron); falls back to Whisper file if no mic
./build/audio --file       # bundled Recording.wav (Whisper)
./build/audio --file /path/to/audio.wav
./build/audio --synth      # generated sine tone (Nemotron streaming)
```

Press `Ctrl+C` to stop live capture gracefully.

