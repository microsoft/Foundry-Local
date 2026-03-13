# Sample: Tool Calling with Foundry Local

This is a simple example of how to use the Foundry Local Rust SDK to run a model locally and perform tool calling with it. The example demonstrates how to set up the SDK, initialize a model, and perform a generated tool call.

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
