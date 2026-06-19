# 🚀 Foundry Local Rust Samples

These samples demonstrate how to use the Rust binding for Foundry Local.
Each sample consumes the SDK from this repository with
`foundry-local-sdk = { path = "../../../sdk/rust" }`, so the samples track
`main` instead of pinning to a published crate version.

## Prerequisites

- [Rust](https://www.rust-lang.org/) 1.70.0 or later

## Samples

| Sample | Description |
|--------|-------------|
| [embeddings](embeddings/) | Generate single and batch text embeddings using the native embedding client. |
| [chat-completion](chat-completion/) | Chat completions with native in-process inference and the local OpenAI-compatible web server. |
| [audio](audio/) | Live microphone streaming with Nemotron ASR plus file-based Whisper transcription. |
| [responses-api](responses-api/) | Stream a vision (image understanding) response from the local web server using the Responses API. |

## Running a Sample

1. Clone the repository:

   ```bash
   git clone https://github.com/microsoft/Foundry-Local.git
   cd Foundry-Local/samples/rust
   ```

2. Run a sample:

   ```bash
   cargo run -p chat-completion
   ```

   Or navigate to a sample directory and run directly:

   ```bash
   cd chat-completion
   cargo run
   ```
