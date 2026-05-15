# Foundry Local Python SDK (v2)

The Foundry Local Python SDK is a native Python binding for the Foundry Local C++ SDK. It lets you discover, download, load, and run inference against local AI models — chat completions (streaming and non-streaming), tool calling, embeddings, and audio transcription — directly in-process via a cffi binding to the Foundry Local native library. No separate service, no HTTP hop.

## Features

- **Model Catalog** – browse and search the Foundry Local model catalog
- **Model Management** – download, cache, load, and unload models
- **Chat Completions** – OpenAI Responses-compatible chat API (streaming and non-streaming)
- **Tool Calling** – first-class function-calling support
- **Embeddings** – text embeddings via OpenAI-compatible API
- **Audio Transcription** – Speech-to-text (offline and live streaming)
- **In-Process Inference** – cffi binding to the Foundry Local native library

## Installation

```bash
pip install foundry-local-sdk
```

The wheel ships the Foundry Local native library and pulls the matching ONNX Runtime + ONNX Runtime GenAI runtime packages as dependencies.

A WinML variant is also published for Windows users who want the Windows ML execution provider:

```bash
pip install foundry-local-sdk-winml
```

The two variants are mutually exclusive — install only one per environment.

## Requirements

- Python 3.11 or newer
- Windows (x64), Linux (x64), or macOS (arm64)

## Quick start

See the [`samples/python/`](https://github.com/microsoft/Foundry-Local/tree/main/samples/python) directory in the repo for runnable end-to-end examples covering chat completions, tool calling, embeddings, and audio transcription.

## License

MIT — see `LICENSE.txt`.

## Links

- [Foundry Local on GitHub](https://github.com/microsoft/Foundry-Local)
- [Issues](https://github.com/microsoft/Foundry-Local/issues)
