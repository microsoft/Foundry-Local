# Foundry Local Web Server Responses Vision (Rust)

Starts the local Foundry web server and streams a vision response from the
OpenAI-compatible Responses API (`/v1/responses`) using a bundled test image by
default.

The sample tracks the Rust SDK from this repository via
`foundry-local-sdk = { path = "../../../sdk/rust" }`, so it follows `main`
instead of pinning to a published crate version.

## Run

```bash
cd samples/rust

# List vision models and variants
cargo run -p responses-api -- --list-models

# Run with a model alias or variant id; omit image_path to use test_image.jpg
cargo run -p responses-api -- qwen3.5-0.8b [image_path]
```
