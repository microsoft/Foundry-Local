# Sample: Live Audio Transcription

This sample demonstrates real-time microphone transcription using the Foundry Local Rust SDK and the `nemotron-speech-streaming-en-0.6b` model.

> **⚠️ Forward-looking sample:** This example requires a Rust SDK version that includes
> `create_live_transcription_session`. The live-transcription session API is not yet
> available in the Rust SDK (`sdk/rust`). This sample is included as a reference for the
> expected API usage and will not compile until the API is added. It is intentionally
> excluded from the workspace `Cargo.toml`.

## Run (once the API is available)

```bash
cd samples/rust/live-audio-transcription-example
cargo run
```

Use synthetic audio instead of a microphone:

```bash
cd samples/rust/live-audio-transcription-example
cargo run -- --synth
```
