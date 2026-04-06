# Foundry Local Playground — Python

> [JavaScript](../js/README.md) · **[Python](.)** · [C#](../cs/README.md) · [Rust](../rust/README.md)

An interactive CLI sample that demonstrates the full Foundry Local Python SDK — from hardware discovery to streaming inference.

## Prerequisites

- [Python](https://www.python.org/downloads/) 3.10 or later

## Setup

```bash
cd python
pip install onnxruntime-core==1.23.2.3 onnxruntime-genai-core==0.13.0
pip install -i https://aiinfra.pkgs.visualstudio.com/PublicPackages/_packaging/ORT-Nightly/pypi/simple/ foundry-local-sdk-winml==1.0.0rc2
```

> **Known issue (Windows + WinML):** Microsoft Store Python is shipped as a packaged app, and packaged apps do not support Windows App SDK bootstrap in this flow. As a result, WinML initialization can fail when using Store Python. Use Python from `python.org` (or a non-Store distribution such as `winget`/`conda`) for WinML scenarios.

> **Note:** On macOS/Linux, install the cross-platform variant instead:
> ```bash
> pip install onnxruntime-core onnxruntime-genai-core
> pip install -i https://aiinfra.pkgs.visualstudio.com/PublicPackages/_packaging/ORT-Nightly/pypi/simple/ foundry-local-sdk==1.0.0rc2
> ```
> No code changes needed — the import is the same.

## Run

```bash
python sample.py
```

## What Happens

1. **Execution providers** are discovered and downloaded with live progress bars.
2. A **model catalog** table is displayed — enter a number to pick a model.
3. The model is **downloaded** (if needed) and **loaded** into memory.
4. Depending on the model type:
   - **Chat models** → interactive conversation with streaming token output in bordered boxes.
   - **Whisper models** → audio transcription — enter a `.wav`/`.mp3` file path and see the transcript stream in.
5. Type `/quit` to exit.

## File Overview

| File | Purpose |
|---|---|
| `sample.py` | Main SDK flow — initialize, discover EPs, browse catalog, load model, run inference |
| `ui.py` | Terminal UI helpers — progress bars, box-drawing tables, streaming output boxes |

## Key SDK APIs Used

```python
from foundry_local_sdk import Configuration, FoundryLocalManager

config = Configuration(app_name="...")
FoundryLocalManager.initialize(config)
manager = FoundryLocalManager.instance

# Execution providers
eps = manager.discover_eps()
result = manager.download_and_register_eps(names=[...], progress_callback=cb)

# Model catalog
models = manager.catalog.list_models()
model  = manager.catalog.get_model(alias)
model.select_variant(variant)

# Download & load
model.download(progress_callback=cb)
model.load()

# Chat inference (streaming)
client = model.get_chat_client()
for chunk in client.complete_streaming_chat(messages):
    print(chunk.choices[0].delta.content)

# Audio transcription (streaming, callback-based)
audio = model.get_audio_client()
audio.transcribe_streaming(file_path, callback=on_chunk)

# Cleanup
model.unload()
```

## Screenshot

```
────────────────────────────────────────────────────────────────────
  Model Catalog
────────────────────────────────────────────────────────────────────
  ┌────┬──────────────┬─────────────────────────┬────────────┬──────────────┬────────┐
  │  # │ Alias        │ Variant                 │ Size (GB)  │ Task         │ Cached │
  ├────┼──────────────┼─────────────────────────┼────────────┼──────────────┼────────┤
  │  1 │ qwen2.5-0.5b │ qwen2.5-0.5b-cpu-4      │        0.4 │ text-gen     │   ●    │
  │  2 │              │ qwen2.5-0.5b-gpu-4      │            │              │   ●    │
  ├────┼──────────────┼─────────────────────────┼────────────┼──────────────┼────────┤
  │  3 │ whisper-tiny │ whisper-tiny-cpu         │        0.1 │ asr          │   ●    │
  └────┴──────────────┴─────────────────────────┴────────────┴──────────────┴────────┘

  Select a model [1-3]:
```
