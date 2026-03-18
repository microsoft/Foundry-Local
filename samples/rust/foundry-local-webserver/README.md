# Sample: Foundry Local Web Server

This example demonstrates how to start a local OpenAI-compatible web server using the Foundry Local SDK, then call it with a standard HTTP client. This is useful when you want to use the OpenAI REST API directly or integrate with tools that expect an OpenAI-compatible endpoint.

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
