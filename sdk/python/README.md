# Foundry Local Python SDK

The Foundry Local Python SDK provides a Python interface for interacting with local AI models via the Foundry Local Core native library. It allows you to discover, download, load, and run inference on models directly on your local machine — no cloud required.

## Features

- **Model Discovery** – browse and search the model catalog
- **Model Management** – download, cache, load, and unload models
- **Chat Completions** – OpenAI-compatible chat API (non-streaming and streaming)
- **Tool Calling** – function-calling support with chat completions
- **Embeddings** – generate text embeddings via OpenAI-compatible API
- **Audio Transcription** – Whisper-based speech-to-text (non-streaming and streaming)
- **Built-in Web Service** – optional HTTP endpoint for multi-process scenarios
- **Native Performance** – ctypes FFI to AOT-compiled Foundry Local Core

## Installation

Two package variants are published — choose the one that matches your target hardware:

| Variant | Package | Native backends |
|---|---|---|
| Standard (cross-platform) | `foundry-local-sdk` | CPU / WebGPU / CUDA |
| WinML (Windows only) | `foundry-local-sdk-winml` | Windows ML + all standard backends |

```bash
# Standard (cross-platform — Linux, macOS, Windows)
pip install foundry-local-sdk

# WinML (Windows only)
pip install foundry-local-sdk-winml
```

Each package installs the correct native binaries (`foundry-local-core`, `onnxruntime-core`, `onnxruntime-genai-core`) as wheel dependencies.  They are mutually exclusive — install only one per environment.  WinML is auto-detected at runtime: if the WinML package is installed, the SDK automatically enables the Windows App Runtime Bootstrap.

### Building from source

```bash
cd sdk/python

# Standard wheel
python -m build --wheel

# WinML wheel (uses the build_backend.py shim)
python -m build --wheel -C winml=true
```

For editable installs during development (native packages installed separately via `foundry-local-install`):

```bash
pip install -e .
```

### Installing native binaries for development / CI

When working from source the native packages are not pulled in automatically.  Use the `foundry-local-install` CLI to install them:

```bash
# Standard
foundry-local-install

# WinML (Windows only)
foundry-local-install --winml
```

Add `--verbose` to print the resolved binary paths after installation:

```bash
foundry-local-install --verbose
foundry-local-install --winml --verbose
```

> **Note:** The standard and WinML native packages use different PyPI package names (`foundry-local-core` vs `foundry-local-core-winml`) so they can coexist in the same pip index, but they should not be installed in the same Python environment simultaneously.

## Explicit EP Management

You can explicitly discover and download execution providers (EPs):

```python
# Discover available EPs and registration status
eps = manager.discover_eps()
for ep in eps:
    print(f"{ep.name} - registered: {ep.is_registered}")

# Download and register all available EPs
result = manager.download_and_register_eps()
print(f"Success: {result.success}, Status: {result.status}")

# Download only specific EPs
result2 = manager.download_and_register_eps([eps[0].name])
```

### Per-EP download progress

Pass a `progress_callback` to receive `(ep_name, percent)` updates as each EP downloads (`percent` is 0–100):

```python
current_ep = ""

def on_progress(ep_name: str, percent: float) -> None:
    global current_ep
    if ep_name != current_ep:
        if current_ep:
            print()
        current_ep = ep_name
    print(f"\r  {ep_name}  {percent:5.1f}%", end="", flush=True)

manager.download_and_register_eps(progress_callback=on_progress)
print()
```

Catalog access does not block on EP downloads. Call `download_and_register_eps()` when you need hardware-accelerated execution providers.

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

### Inspecting Model Metadata

`IModel` exposes metadata properties from the catalog:

```python
model = catalog.get_model("phi-3.5-mini")

# Identity
print(model.id)             # e.g. "phi-3.5-mini-instruct-generic-gpu:3"
print(model.alias)          # e.g. "phi-3.5-mini"

# Context and token limits
print(model.context_length) # e.g. 131072 (tokens), or None if unknown

# Modalities and capabilities
print(model.input_modalities)   # e.g. "text" or "text,image"
print(model.output_modalities)  # e.g. "text"
print(model.capabilities)       # e.g. "chat,completion"
print(model.supports_tool_calling)  # True, False, or None

# Cache / load state
print(model.is_cached)
print(model.is_loaded)
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
client.settings.max_tokens = 500

result = client.complete_chat([
    {"role": "user", "content": "What is 7 multiplied by 6?"}
])
print(result.choices[0].message.content)  # "42"

# Streaming chat
messages = [{"role": "user", "content": "Tell me a joke"}]

for chunk in client.complete_streaming_chat(messages):
    if chunk.choices[0].delta.content:
        print(chunk.choices[0].delta.content, end="", flush=True)

# Unload when done
model.unload()
```

### Embeddings

Generate text embeddings using the `EmbeddingClient`:

```python
embedding_client = model.get_embedding_client()

# Single input
response = embedding_client.generate_embedding(
    "The quick brown fox jumps over the lazy dog"
)
embedding = response.data[0].embedding  # List[float]
print(f"Dimensions: {len(embedding)}")

# Batch input
batch_response = embedding_client.generate_embeddings([
    "The quick brown fox",
    "The capital of France is Paris"
])
# batch_response.data[0].embedding, batch_response.data[1].embedding
```

#### Embedding Settings

```python
embedding_client.settings.encoding_format = "float" # "float" (default) or "base64"
```

> **Note:** `encoding_format` only takes effect when using the HTTP web service endpoint. SDK clients always receive float arrays. The `dimensions` setting is not currently supported — the full embedding vector is always returned.

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
| `EpInfo` | Discoverable execution provider info (`name`, `is_registered`) |
| `EpDownloadResult` | Result of EP download/registration (`success`, `status`, `registered_eps`, `failed_eps`) |
| `Catalog` | Model discovery – listing, lookup by alias/ID, cached/loaded queries |
| `IModel` | Abstract interface for models — identity, metadata, lifecycle, client creation, variant selection |

### OpenAI Clients

| Class | Description |
|---|---|
| `ChatClient` | Chat completions (non-streaming and streaming) with tool calling |
| `EmbeddingClient` | Text embedding generation via OpenAI-compatible API |
| `AudioClient` | Audio transcription (non-streaming and streaming) |

### Internal / Detail

| Class | Description |
|---|---|
| `Model` | Alias-level `IModel` implementation used by `Catalog.get_model()` (implementation detail) |
| `ModelVariant` | Specific model variant (implementation detail — implements `IModel`) |
| `CoreInterop` | ctypes FFI layer to the native Foundry Local Core library |
| `ModelLoadManager` | Load/unload via core interop or external web service |
| `ModelInfo` | Pydantic model for catalog entries |

### CLI entry point

| Function | CLI name | Description |
|---|---|---|
| `foundry_local_sdk.detail.utils.foundry_local_install` | `foundry-local-install` | Install and verify native binaries (`--winml` for WinML variant) |

> **Migration note:** The function was previously named `verify_native_install`.  The public CLI name (`foundry-local-install`) and its behaviour are unchanged; only the Python function name in `foundry_local_sdk.detail.utils` was updated to `foundry_local_install` for consistency.

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