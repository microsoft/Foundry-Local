# Live Audio Transcription (Rust)

Demonstrates both audio transcription paths in one sample:

- live microphone streaming with the Nemotron ASR model (default)
- file-based Whisper transcription with `--file`

The sample tracks the Rust SDK from this repository via
`foundry-local-sdk = { path = "../../../sdk/rust" }`, so it follows `main`
instead of pinning to a published crate version.

## Run

```bash
cd samples/rust

# Live microphone (press Ctrl+C to stop)
cargo run -p audio

# Synthetic 440Hz sine wave (no microphone needed)
cargo run -p audio -- --synth

# Transcribe the bundled Recording.mp3 with Whisper
cargo run -p audio -- --file

# Transcribe a custom audio file with Whisper
cargo run -p audio -- --file path/to/audio.mp3
```
