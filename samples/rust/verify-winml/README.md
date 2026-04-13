# Verify WinML 2.0 Execution Providers (Rust)

This sample verifies that WinML 2.0 execution providers are correctly discovered,
downloaded, and registered using the Foundry Local Rust SDK. It uses registered WinML
EP-backed model variants and finishes with one native streaming chat check.

## Prerequisites

- Windows with a compatible GPU
- Windows App SDK 2.0 runtime installed (preview1 or experimental)
- Rust toolchain

## Build & Run

This sample enables the Rust SDK's `winml` feature and the SDK build script
downloads the preview `Microsoft.AI.Foundry.Local.Core.WinML` package from
ORT-Nightly during the build.

```bash
cargo run
```
