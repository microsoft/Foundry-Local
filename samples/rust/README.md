# Foundry Local Rust samples

These five samples demonstrate the supported Foundry Local Rust SDK workflows.
All crates use the SDK in `sdk_v2/rust` and set the application name to
`foundry_local_samples`.

> [!IMPORTANT]
> These samples track the latest Rust SDK on `main` through a local path dependency and require Foundry Local to be
> built or acquired by the SDK before they can run. Installing
> [`foundry-local-sdk` from crates.io](https://crates.io/crates/foundry-local-sdk) is easier, but these samples are not
> built or tested against a specific published version. See the
> [get-started documentation](https://learn.microsoft.com/en-us/azure/foundry-local/get-started) for versioned setup
> guidance and additional examples.

## Prerequisites

- A supported Foundry Local platform
- The Rust toolchain

## Samples

| Directory | Demonstrates |
| --- | --- |
| [`hello-world`](hello-world/) | Manager setup, execution-provider registration, catalog/model lifecycle, and native chat inference |
| [`realtime-transcription`](realtime-transcription/) | In-process microphone streaming with the temporarily retained deprecated live-transcription API |
| [`embeddings`](embeddings/) | Single and batch embeddings with the native `EmbeddingsSession` |
| [`tool-calling`](tool-calling/) | Native `ChatSession` tool definitions, tool-call items, and tool-result items |
| [`web-server`](web-server/) | Starting the built-in service, discovering its URL, calling an OpenAI-compatible endpoint, and stopping it |

## Running a Sample

Run a package from this directory:

```bash
cd samples/rust
cargo run -p hello-world
```

Replace `hello-world` with any package name from the table. Real-time
transcription uses the default microphone and runs until Ctrl+C:

```bash
cargo run -p realtime-transcription
```

The first run may download execution providers and model assets. Later runs
reuse cached models.
