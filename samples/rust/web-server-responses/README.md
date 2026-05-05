# Responses API web-service sample

This sample starts the Foundry Local OpenAI-compatible web service with the Rust SDK, then calls the Responses API through raw HTTP requests to `/v1/responses`.

It demonstrates:

- Non-streaming Responses API calls
- Streaming Server-Sent Events (SSE) responses
- Function/tool calling with `previous_response_id`
- Local model load/unload and web-service cleanup

## Prerequisites

- Rust 1.70 or later
- Foundry Local runtime prerequisites for your platform
- Internet access the first time dependencies, execution providers, or the sample model need to be downloaded

No OpenAI API key is required. The sample talks to the local Foundry Local web service.

## What gets installed

Cargo restores the Rust crates declared in `Cargo.toml`:

| Dependency | Purpose |
|------------|---------|
| `foundry-local-sdk` | Initializes Foundry Local, downloads/registers execution providers, manages the model, and starts/stops the local web service. |
| `tokio` | Runs the async sample. |
| `reqwest` | Sends JSON requests and reads streaming SSE chunks from `/v1/responses`. |
| `serde_json` | Builds request payloads and reads response JSON. |

On Windows, the sample enables the SDK `winml` feature through the target-specific dependency in `Cargo.toml`.

At runtime, the sample also:

- Downloads and registers Foundry Local execution providers if needed.
- Downloads `qwen2.5-0.5b` if it is not already cached.
- Starts the local OpenAI-compatible web service and uses the dynamic URL returned by the SDK.

Downloaded models, native runtime files, and Cargo build outputs are local machine artifacts and should not be committed.

## Run the sample

From the Rust samples workspace:

```powershell
cd samples\rust
cargo run -p web-server-responses
```

Or from this sample directory:

```powershell
cd samples\rust\web-server-responses
cargo run
```

The sample prints progress for execution-provider/model setup, then runs:

1. A non-streaming Responses request.
2. A streaming Responses request that consumes `response.output_text.delta` events.
3. A function-calling request that asks the model to call `get_weather`, submits a `function_call_output`, and prints the final assistant response.

## Troubleshooting

If setup fails while resolving native Foundry Local symbols, verify that your locally installed Foundry Local runtime packages are compatible with the SDK version in this repository.

If model download is unavailable, pre-cache `qwen2.5-0.5b` with your normal Foundry Local workflow, then run the sample again.
