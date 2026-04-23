# Sample: Live Audio Transcription

This sample demonstrates real-time microphone transcription using the Foundry Local Rust SDK and the `nemotron` model.

> This example requires a Rust SDK version that includes `create_live_transcription_session`.

## Run

```bash
cargo run -p live-audio-transcription-example
```

Use synthetic audio instead of a microphone:

```bash
cargo run -p live-audio-transcription-example -- --synth
```
