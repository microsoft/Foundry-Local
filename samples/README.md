# Foundry Local Samples

These focused examples demonstrate the Foundry Local v2 SDKs with the same five capabilities in every supported
language. Except for the web-server sample, inference runs natively in the application process.

> **New to Foundry Local?** Start with the [main README](../README.md), then choose a language below.

> [!IMPORTANT]
> These samples track the latest SDK APIs on `main` and reference the SDK source in this repository. Build Foundry
> Local from source before running them. Installing a published package from NuGet, npm, PyPI, or crates.io is easier
> for most users, but these samples are not built or tested against a specific published package version and may use
> newer APIs. For versioned setup instructions and more examples, see the
> [Foundry Local get-started documentation](https://learn.microsoft.com/en-us/azure/foundry-local/get-started).

## Languages

| Language | SDK | Samples |
|----------|-----|---------|
| [C++](cpp/) | [`sdk_v2/cpp`](../sdk_v2/cpp/) | 5 |
| [C#](cs/) | [`sdk_v2/cs`](../sdk_v2/cs/) | 5 |
| [JavaScript](js/) | [`sdk_v2/js`](../sdk_v2/js/) | 5 |
| [Python](python/) | [`sdk_v2/python`](../sdk_v2/python/) | 5 |
| [Rust](rust/) | [`sdk_v2/rust`](../sdk_v2/rust/) | 5 |

## Capabilities

Every language contains:

| Sample | Demonstrates |
|--------|--------------|
| `hello-world` | Manager creation, execution-provider setup, catalog/model selection, download, load, native chat inference, and cleanup |
| `realtime-transcription` | Native, in-process streaming speech transcription |
| `embeddings` | Native embedding generation and similarity comparison |
| `tool-calling` | Native tool definitions, tool execution, and multi-turn continuation |
| `web-server` | Starting and calling the built-in OpenAI-compatible HTTP server |

All samples use `foundry_local_samples` as the application name so they share the same local model cache.
Each sample registers execution providers before selecting its model. Hello world uses automatic model selection
and a streaming chat request about the sky; embeddings compares three sentences about cats and Saturn; tool calling
requests Paris weather and returns a mock result before a final answer with tools disabled. Microphone capture and
resource cleanup use the conventions of each language.
