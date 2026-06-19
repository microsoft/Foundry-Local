# Native + Web Server Chat Completions (Rust)

Runs the same prompt two ways:

1. native in-process chat completion with `model.create_chat_client()`
2. OpenAI-compatible `/v1/chat/completions` through the local Foundry web server

The sample tracks the Rust SDK from this repository via
`foundry-local-sdk = { path = "../../../sdk/rust" }`, so it follows `main`
instead of pinning to a published crate version.

## Run

```bash
cd samples/rust
cargo run -p chat-completion
```
