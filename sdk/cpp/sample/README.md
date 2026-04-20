# Responses API with Qwen 3.5 — C++ Sample

A sample app demonstrating the Foundry Local C++ SDK Responses API with the Qwen
3.5 multimodal model. Supports text prompts and optional image input with
streaming output.

The default model is `qwen3.5-4b`. The following Qwen 3.5 variants are also supported:

| Model | Alias |
|-------|-------|
| Qwen 3.5 0.8B | `qwen3.5-0.8b` |
| Qwen 3.5 2B | `qwen3.5-2b` |
| Qwen 3.5 4B | `qwen3.5-4b` |
| Qwen 3.5 9B | `qwen3.5-9b` |

To use a different variant, update the `MODEL_ALIAS` constant in `responses_api_qwen.cpp`.

## Prerequisites

- **Windows 10/11** (x64 or ARM64)
- **Visual Studio 2022** (or Build Tools) with the **C++ Desktop** workload
- **CMake** 3.20 or later
- **[vcpkg](https://github.com/microsoft/vcpkg)** — with `VCPKG_ROOT` environment variable set
- **Foundry Local** installed (`winget install Microsoft.FoundryLocal`)

> **Tip:** If `cmake` is not recognized, run these commands from **Developer Command Prompt for VS 2022** instead, which adds cmake to PATH automatically.

## Setup

### x64

```powershell
# 1. Clone the repo (if not already)
git clone -b Wayne-Ch/external-delivery https://github.com/microsoft/Foundry-Local.git
cd Foundry-Local/sdk/cpp

# 2. Configure (downloads vcpkg dependencies + native DLLs automatically)
cmake -B out/build/x64-debug-S . -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static-md `
  -DVCPKG_OVERLAY_TRIPLETS=triplets

# 3. Build
cmake --build out/build/x64-debug --config Debug
```

### ARM64

```powershell
# 1. Clone the repo (if not already)
git clone -b Wayne-Ch/external-delivery https://github.com/microsoft/Foundry-Local.git
cd Foundry-Local/sdk/cpp

# 2. Configure for ARM64
cmake -B out/build/arm64-debug -S . -G "Visual Studio 17 2022" -A ARM64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=arm64-windows-static-md `
  -DVCPKG_OVERLAY_TRIPLETS=triplets

# 3. Build
cmake --build out/build/arm64-debug --config Debug
```

The build automatically downloads the required native dependencies (FLCore,
OnnxRuntime, OnnxRuntimeGenAI) from the ORT-Nightly NuGet feed and copies them
next to the executable. No manual DLL management needed.

To skip the auto-download (e.g. if you have Foundry Local installed locally):

```powershell
cmake ... -DFETCH_NATIVE_DEPS=OFF
```

## Usage

> Replace `x64-debug` with `arm64-debug` in paths below if building for ARM64.

### Text prompt

```powershell
.\out\build\x64-debug\Debug\CppSdkResponsesApiQwen.exe "What is quantum computing?"
```

### Text + image (local file)

```powershell
.\out\build\x64-debug\Debug\CppSdkResponsesApiQwen.exe "Describe this image" --image path/to/photo.png
```

### Text + image (URL)

```powershell
.\out\build\x64-debug\Debug\CppSdkResponsesApiQwen.exe "What do you see?" --image https://example.com/image.png
```

### Check model cache

```powershell
.\out\build\x64-debug\Debug\CppSdkResponsesApiQwen.exe --check-cache
```

## How it works

1. Initializes the Foundry Local C++ SDK
2. Looks up `qwen3.5-4b` in the catalog, selects the CPU variant
3. Downloads the model if not cached, then loads it
4. Starts the embedded web service
5. Runs inference with streaming output via the Responses API (`/v1/responses`)
   - Text-only → `OpenAIResponsesClient::CreateStreaming()` with text input
   - Text + image → `OpenAIResponsesClient::CreateStreaming()` with `input_image` content part

## Project structure

```
sdk/cpp/
├── cmake/
│   └── FetchNativeDeps.cmake      # Auto-downloads native DLLs from NuGet
├── include/
│   ├── foundry_local.h            # Umbrella header
│   └── openai/
│       ├── openai_responses_client.h   # ResponsesClient public API
│       └── openai_responses_types.h    # Types (ResponseObject, StreamingEvent, etc.)
├── src/
│   ├── openai_responses_client.cpp # Client implementation
│   └── http_helpers.h              # WinHTTP + SSE streaming helpers
├── sample/
│   ├── main.cpp                    # Original SDK sample
│   └── responses_api_qwen.cpp      # This sample
└── CMakeLists.txt
```

## Notes

- The sample automatically selects a **CPU variant** to avoid GPU compatibility issues.
- Image support requires ORT-GenAI v0.13.0+ for vision models.
- The model is automatically cached after first download. Subsequent runs skip the download.
- Supported image formats: JPEG, PNG, GIF, WebP, BMP.
- Native dependency versions are read from `sdk/deps_versions.json` (shared with JS SDK).
