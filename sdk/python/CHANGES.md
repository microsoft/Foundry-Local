# Python SDK: C# Interop → C++ C ABI Migration

Replace the legacy Python SDK's C# interop layer (`CoreInterop` + ctypes against `Microsoft.AI.Foundry.Local.Core.dll`) with the C++ C ABI via cffi (against `foundry_local.dll`), reusing the production-tested `sdk_v2` implementation.

## What changed

The SDK now calls the C++ native library (`foundry_local.dll`) directly through cffi instead of going through a C# AOT-compiled shim. The public API surface is **unchanged** — existing code using `FoundryLocalManager`, `Catalog`, `IModel`, `ChatClient`, etc. continues to work without modification.

New native types (`Item`, `TextItem`, `MessageItem`, `Session`, `Request`, `Response`, etc.) are exported for advanced callers who want to bypass the OpenAI-compat layer.

## Files

### Added (copied from `sdk_v2/python/` with no modifications)

| File | Source | Purpose |
|------|--------|---------|
| `setup.py` | new | Declares `cffi_modules` for extension compilation |
| `src/_native/__init__.py` | `sdk_v2/.../foundry_local_sdk/_native/__init__.py` | cffi package init; exposes `api` and `ffi` |
| `src/_native/api.py` | `sdk_v2/.../foundry_local_sdk/_native/api.py` | `_Api` singleton wrapping the `flApi` vtable from `FoundryLocalGetApi()` |
| `src/_native/build_cffi.py` | `sdk_v2/.../foundry_local_sdk/_native/build_cffi.py` | cffi cdef block matching `foundry_local_c.h`; compiles `_cffi_bindings.abi3.pyd` |
| `src/_native/lib_loader.py` | `sdk_v2/.../foundry_local_sdk/_native/lib_loader.py` | Runtime discovery of `foundry_local.dll` (env var → wheel-bundled → dev build → system) |
| `src/items.py` | `sdk_v2/.../foundry_local_sdk/items.py` | Item type hierarchy: `TextItem`, `MessageItem`, `BytesItem`, `ImageItem`, `AudioItem`, `ToolCallItem`, `ToolResultItem`, `TensorItem` |
| `src/item_queue.py` | `sdk_v2/.../foundry_local_sdk/item_queue.py` | `ItemQueue` for streaming input (live audio) |
| `src/request.py` | `sdk_v2/.../foundry_local_sdk/request.py` | `Request` wrapper around `flRequest*` |
| `src/response.py` | `sdk_v2/.../foundry_local_sdk/response.py` | `Response` wrapper around `flResponse*` |
| `src/session.py` | `sdk_v2/.../foundry_local_sdk/session.py` | `Session`, `ChatSession`, `AudioSession`, `EmbeddingsSession` with streaming support |
| `src/session_types.py` | `sdk_v2/.../foundry_local_sdk/session_types.py` | `FinishReason`, `TokenUsage`, `SessionParam` |
| `src/model_info.py` | `sdk_v2/.../foundry_local_sdk/model_info.py` | `ModelInfo` dataclass, `DeviceType`, `Runtime`, `Parameter`, `ModelSettings` |

### Modified (replaced with `sdk_v2` equivalents)

| File | What changed |
|------|-------------|
| `src/__init__.py` | Exports all new native types (`Item`, `Session`, `Request`, `Response`, etc.) alongside existing public API |
| `src/catalog.py` | Replaced CoreInterop string commands with native `flCatalogApi` vtable calls |
| `src/configuration.py` | Added `_build_native()` method that constructs `flConfiguration*` via `flConfigurationApi` setters |
| `src/ep_types.py` | Switched from Pydantic models to plain `@dataclass(frozen=True)` (no more `Field(alias=...)`) |
| `src/exception.py` | Added `error_code` attribute to `FoundryLocalException` |
| `src/foundry_local_manager.py` | Replaced CoreInterop init with `Manager_Create`/`Manager_GetCatalog`/`Manager_Shutdown`/`Manager_Release` vtable calls; EP discovery via parallel arrays |
| `src/imodel.py` | Added `_ModelImpl` class wrapping `flModel*` with native vtable calls for download/load/unload/variants; added `_model_info_from_native()` |
| `src/logging_helper.py` | Minor: added `set_default_logger_severity` type annotation |
| `src/openai/__init__.py` | Added `AudioSettings` export |
| `src/openai/chat_client.py` | Replaced `execute_command("chat_completions")` with `ChatSession` + `TextItem(OPENAI_JSON)` + `process_request()` |
| `src/openai/audio_client.py` | Replaced `execute_command("audio_transcribe")` with `AudioSession` + `TextItem(OPENAI_JSON)` |
| `src/openai/embedding_client.py` | Replaced `execute_command("embeddings")` with `EmbeddingsSession` + `TextItem(OPENAI_JSON)` |
| `src/openai/live_audio_session.py` | Replaced `audio_stream_start/push/stop` commands with `ItemQueue` streaming pattern |
| `src/openai/live_audio_types.py` | Switched from Pydantic to plain dataclasses |

### Modified (build system)

| File | What changed |
|------|-------------|
| `pyproject.toml` | Added `cffi>=1.16` to build-requires; added `_native` package; removed `detail` package and `foundry-local-install` entry point |
| `requirements.txt` | Added `cffi>=1.16`; removed `foundry-local-core==1.1.0` dependency |
| `requirements-base.txt` | Added `cffi>=1.16` |
| `build_backend.py` | Removed `foundry-local-core` / `foundry-local-core-winml` from generated requirements |

### Deleted

| File | What it contained |
|------|------------------|
| `src/detail/__init__.py` | Re-exports of `CoreInterop`, `ModelInfo`, `ModelLoadManager` |
| `src/detail/core_interop.py` | C# DLL loading via ctypes, `InteropRequest`, `RequestBuffer`, `ResponseBuffer`, `CallbackHelper`, `execute_command()` |
| `src/detail/model.py` | `Model` container grouping variants by alias |
| `src/detail/model_data_types.py` | Pydantic `ModelInfo` with JSON field aliases |
| `src/detail/model_load_manager.py` | Model load/unload orchestration via CoreInterop string commands |
| `src/detail/model_variant.py` | `ModelVariant` wrapping CoreInterop for download/load/unload |
| `src/detail/utils.py` | `get_native_binary_paths()` for C# DLL discovery, `create_ort_symlinks()`, `foundry_local_install` CLI entry point |

## Dependency changes

| Before | After |
|--------|-------|
| `foundry-local-core==1.1.0` (C# AOT wheel) | **removed** — native lib bundled directly or discovered from dev build |
| `pydantic>=2.0.0` | `pydantic>=2.0.0` (still used by OpenAI types) |
| — | `cffi>=1.16` (new: compiles C ABI bindings) |

## Local dev workflow

```powershell
# 1. Build C++ native library
cd Foundry-Local/sdk_v2/cpp
python build.py --skip_tests --skip_service

# 2. Install SDK (editable)
cd Foundry-Local/sdk/python
pip install -e .

# 3. Run example
python examples/chat_completion.py
```
