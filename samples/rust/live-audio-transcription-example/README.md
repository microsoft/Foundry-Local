# Live Audio Transcription Example (Rust)

Demonstrates real-time microphone-to-text using the Foundry Local Rust SDK:

**Microphone (CPAL) → SDK (FoundryLocalManager) → Core (NativeAOT DLL)**

Uses [CPAL](https://crates.io/crates/cpal) for cross-platform microphone capture
(the Rust equivalent of `naudiodon2` in JS / `PortAudio` in C++ / `PyAudio` in Python).
If CPAL cannot open a microphone, falls back to synthetic PCM audio.

> **⚠️ Forward-looking sample:** The live-transcription session API
> (`create_live_transcription_session`) is not yet available in the Rust SDK.
> This sample is included as a reference and will not compile until the API is added.
> It is intentionally excluded from the workspace `Cargo.toml`.

## Run (once the API is available)

```bash
cd samples/rust/live-audio-transcription-example

# Live microphone (press Ctrl+C to stop)
cargo run

# Synthetic 440Hz sine wave (no microphone needed)
cargo run -- --synth
```
