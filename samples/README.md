# Foundry Local Samples

A small, focused set of working examples for [Foundry Local](https://learn.microsoft.com/azure/foundry-local/) — an end-to-end local AI solution that runs entirely on-device.

> **New to Foundry Local?** Check out the [main README](../README.md) for an overview and quickstart, or visit the [Foundry Local documentation](https://learn.microsoft.com/azure/foundry-local/) on Microsoft Learn.

## These samples track `main`

Every sample here **consumes the SDK from local source in this repository** and therefore
reflects the current state of `main` — they are intentionally **not pinned** to a published
package version. Concretely:

| Language | How the SDK is consumed | Built from |
|----------|-------------------------|------------|
| **C#** | `ProjectReference` to `sdk/cs/src/Microsoft.AI.Foundry.Local.csproj` | `sdk/cs` |
| **JavaScript** | `file:` dependency on the local SDK (`foundry-local-sdk`) | `sdk/js` |
| **Python** | editable install (`-e ../../../sdk/python`) in `requirements.txt` | `sdk/python` |
| **Rust** | `path` dependency (`foundry-local-sdk = { path = "../../../sdk/rust" }`) | `sdk/rust` |
| **C++** | links the locally built `foundry_local_cpp` library | `sdk_v2/cpp` |

> Build the relevant SDK first (see each sample's README), then build/run the sample. Because
> the samples reference local source, there is no version to bump — they always use the code
> currently checked out.

> **What "local source" means precisely:** the Foundry Local **SDK binding** always resolves to
> the in-repo source above — never a published PyPI/npm/crates/NuGet release. Only the
> third-party **native runtime** (ONNX Runtime / GenAI / Foundry Core native) is fetched from
> public package feeds, exactly as each SDK itself obtains it.

> **Hardware acceleration (WinML):** for simplicity and consistency, these samples use the
> standard cross-platform SDK on **all** platforms (Windows, macOS, Linux). Windows hardware
> acceleration via WinML is a capability of the SDK itself, not wired into these samples — see
> the [main README](../README.md) quickstart to enable it in your own app.

## Want version-pinned or comprehensive samples?

For a broader catalog of samples pinned to specific released versions — including the examples
referenced from **Microsoft Learn** content — see:

> 👉 **[microsoft-foundry/foundry-samples](https://github.com/microsoft-foundry/foundry-samples/)**

## What's included

Each language provides the same four samples:

| Sample | Description |
|--------|-------------|
| **chat** (`chat-completion`) | Runs a prompt through **native in-process inference**, then the **same prompt over the local web server** (OpenAI-compatible `/v1/chat/completions`). |
| **embeddings** (`embeddings`) | Generates text embeddings (single and batch) using the native SDK. |
| **audio** (`audio`) | **Live** microphone streaming transcription (Nemotron ASR) by default, plus **file-based** transcription (Whisper) via `--file <path>`. |
| **responses** (`responses-api`) | Vision (image understanding) via the local web server **Responses API** (`/v1/responses`). |

## Samples by Language

| Language | Folder | Notes |
|----------|--------|-------|
| **C#** | [`cs/`](cs/) | .NET SDK. |
| **JavaScript** | [`js/`](js/) | Node.js SDK. |
| **Python** | [`python/`](python/) | Python SDK (OpenAI-compatible API for web-server samples). |
| **Rust** | [`rust/`](rust/) | Rust SDK (Cargo workspace). |
| **C++** | [`cpp/`](cpp/) | C++ SDK (`sdk_v2/cpp`); build the SDK with `python sdk_v2/cpp/build.py` first. |
