# Sample: Native Chat Completions

This example demonstrates both non-streaming and streaming chat completions using the Foundry Local Rust SDK's native chat client — no external HTTP libraries needed.

The `foundry-local-sdk` dependency is referenced via a local path. No crates.io publish is required:

```toml
foundry-local-sdk = { path = "../../../sdk_v2/rust" }
```

Run the application:

```bash
cargo run
```

## Using WinML (Windows only)

To use the WinML backend, enable the `winml` feature in `Cargo.toml`:

```toml
foundry-local-sdk = { path = "../../../sdk_v2/rust", features = ["winml"] }
```

No code changes are needed — same API, different backend.
