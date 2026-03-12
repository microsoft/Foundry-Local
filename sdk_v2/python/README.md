# Foundry Local Python SDK

The Foundry Local Python SDK provides a Python interface for interacting with local AI models via the Foundry Local Core native library. It allows you to discover, download, load, and run inference on models directly on your local machine — no cloud required.

## Features

- **Model Discovery** – browse and search the model catalog
- **Model Management** – download, cache, load, and unload models
- **Chat Completions** – OpenAI-compatible chat API (non-streaming and streaming)
- **Tool Calling** – function-calling support with chat completions
- **Audio Transcription** – Whisper-based speech-to-text (non-streaming and streaming)
- **Built-in Web Service** – optional HTTP endpoint for multi-process scenarios
- **Native Performance** – ctypes FFI to AOT-compiled Foundry Local Core

## Installation

```bash
pip install foundry-local-sdk
```

### Building from source

```bash
cd sdk_v2/python
pip install -e .
```

### WinML Binaries (Windows Only)

To use WinML execution providers, install the winml native binaries instead of the default cross-plat ones:

```bash
foundry-local-install --winml
```

This downloads the winml variants of the Foundry Local Core, OnnxRuntime, and OnnxRuntimeGenAI packages. WinML is only supported on Windows.

You can also combine flags:

```bash
# winml with nightly builds
foundry-local-install --winml --nightly
```

> **Note:** winml and cross-plat binaries cannot coexist in the same target directory. Running `foundry-local-install --winml` will overwrite any previously installed cross-plat binaries (and vice versa).

## Quick Start

```python
from foundry_local_sdk import Configuration, FoundryLocalManager

# 1. Initialize
config = Configuration(app_name="MyApp")
FoundryLocalManager.initialize(config)
manager = FoundryLocalManager.instance

# 2. Discover models
catalog = manager.catalog
models = catalog.list_models()
for m in models:
    print(f"  {m.alias}")

# 3. Load a model
model = catalog.get_model("phi-3.5-mini")
model.load()

# 4. Chat
client = model.get_chat_client()
response = client.complete_chat([
    {"role": "user", "content": "Why is the sky blue?"}
])
print(response.choices[0].message.content)

# 5. Cleanup
model.unload()
```

## Usage

### Initialization

Create a `Configuration` and initialize the singleton `FoundryLocalManager`.

```python
from foundry_local_sdk import Configuration, FoundryLocalManager
from foundry_local_sdk.configuration import LogLevel

config = Configuration(
    app_name="MyApp",
    model_cache_dir="/path/to/cache",     # optional
    log_level=LogLevel.INFORMATION,        # optional (default: Warning)
    additional_settings={"Bootstrap": "false"},  # optional
)
FoundryLocalManager.initialize(config)
manager = FoundryLocalManager.instance
```

### Discovering Models

```python
catalog = manager.catalog

# List all models in the catalog
models = catalog.list_models()

# Get a specific model by alias
model = catalog.get_model("qwen2.5-0.5b")

# Get a specific variant by ID
variant = catalog.get_model_variant("qwen2.5-0.5b-instruct-generic-cpu:4")

# List locally cached models
cached = catalog.get_cached_models()

# List currently loaded models
loaded = catalog.get_loaded_models()
```

### Loading and Running a Model

```python
model = catalog.get_model("qwen2.5-0.5b")

# Select a specific variant (optional – defaults to highest-priority cached variant)
cached = catalog.get_cached_models()
variant = next(v for v in cached if v.alias == "qwen2.5-0.5b")
model.select_variant(variant)

# Load into memory
model.load()

# Non-streaming chat
client = model.get_chat_client()
client.settings.temperature = 0.0
client.settings.max_completion_tokens = 500

result = client.complete_chat([
    {"role": "user", "content": "What is 7 multiplied by 6?"}
])
print(result.choices[0].message.content)  # "42"

# Streaming chat
messages = [{"role": "user", "content": "Tell me a joke"}]

def on_chunk(chunk):
    delta = chunk.choices[0].delta
    if delta and delta.content:
        print(delta.content, end="", flush=True)

client.complete_streaming_chat(messages, on_chunk)

# Unload when done
model.unload()
```

### Web Service (Optional)

Start a built-in HTTP server for multi-process access.

```python
manager.start_web_service()
print(f"Listening on: {manager.urls}")

# ... use the service ...

manager.stop_web_service()
```

## API Reference

### Core Classes

| Class | Description |
|---|---|
| `Configuration` | SDK configuration (app name, cache dir, log level, web service settings) |
| `FoundryLocalManager` | Singleton entry point – initialization, catalog access, web service |
| `Catalog` | Model discovery – listing, lookup by alias/ID, cached/loaded queries |
| `Model` | Groups variants under one alias – select, load, unload, create clients |
| `ModelVariant` | Specific model variant – download, cache, load/unload, create clients |

### OpenAI Clients

| Class | Description |
|---|---|
| `ChatClient` | Chat completions (non-streaming and streaming) with tool calling |
| `AudioClient` | Audio transcription (non-streaming and streaming) |

### Internal / Detail

| Class | Description |
|---|---|
| `CoreInterop` | ctypes FFI layer to the native Foundry Local Core library |
| `ModelLoadManager` | Load/unload via core interop or external web service |
| `ModelInfo` | Pydantic model for catalog entries |

## Running Tests

```bash
pip install -r requirements-dev.txt
python -m pytest test/ -v
```

See [test/README.md](test/README.md) for detailed test setup and structure.

## Running Examples

```bash
python examples/chat_completion.py
```