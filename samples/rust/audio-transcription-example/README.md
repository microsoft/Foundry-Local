# Sample: Audio Transcription

This example demonstrates audio transcription (non-streaming and streaming) using the Foundry Local Rust SDK. It uses the `whisper` model to transcribe a WAV audio file.

The `foundry-local-sdk` dependency is referenced via a local path. No crates.io publish is required:

```toml
foundry-local-sdk = { path = "../../../sdk/rust" }
```

Run the application with a path to a WAV file:

```bash
cargo run -- path/to/audio.wav
```

## Using WinML (Windows only)

To use the WinML backend, enable the `winml` feature in `Cargo.toml`:

```toml
foundry-local-sdk = { path = "../../../sdk/rust", features = ["winml"] }
```

No code changes are needed — same API, different backend.
