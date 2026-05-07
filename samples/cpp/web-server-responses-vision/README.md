# Foundry Local C++ Vision Sample (Responses API)

This sample demonstrates vision (image understanding) capabilities using the Foundry Local web service and the OpenAI Responses API.

> **Windows-only** — requires MSVC or clang-cl (MSVC-compatible toolchain).

## Features

- **Vision inference** — send an image to a vision-capable model and get a description
- **Streaming** — token-by-token output via Server-Sent Events (SSE)
- **Responses API** — uses the `/v1/responses` endpoint (not chat completions)
- Uses a default test image (`test_image.jpg`) if no image path is provided

## Prerequisites

| Requirement | Notes |
|---|---|
| **Foundry Local / AI Toolkit** | Install via `winget install Microsoft.AIToolkit` or the VS Code AI Toolkit extension |
| **CMake >= 3.20** | Ships with Visual Studio 2022 |
| **Ninja** | Ships with Visual Studio 2022 |
| **vcpkg** | Set the `VCPKG_ROOT` environment variable to your vcpkg installation |
| **MSVC** (or clang-cl) | Visual Studio 2022 Build Tools or full IDE |

The sample downloads the specified model the first time it runs (skips if already cached).

## Build

Open an **x64 Native Tools Command Prompt for VS 2022** (or run `vcvars64.bat`), then navigate to the sample directory:

```bash
cd samples/cpp/web-server-responses-vision
```

### 1. Download native dependencies

Download the required NuGet packages to `_native_deps` (needed for runtime DLLs):

```bash
nuget install Microsoft.AI.Foundry.Local.Core -Version 1.1.0 -OutputDirectory _native_deps
nuget install Microsoft.ML.OnnxRuntime.Foundry -Version 1.25.1 -OutputDirectory _native_deps
nuget install Microsoft.ML.OnnxRuntimeGenAI.Foundry -Version 0.13.2 -OutputDirectory _native_deps
```

### 2. Build

```bash
cmake -G Ninja -B build -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows-static-md
cmake --build build
```

The built executable will be at `build/web-server-responses-vision.exe`.

### 3. Copy runtime DLLs

Copy the `win-x64` DLLs next to the executable:

```bash
copy _native_deps\Microsoft.AI.Foundry.Local.Core.1.1.0\runtimes\win-x64\native\*.dll build\
copy _native_deps\Microsoft.ML.OnnxRuntime.Foundry.1.25.1\runtimes\win-x64\native\*.dll build\
copy _native_deps\Microsoft.ML.OnnxRuntimeGenAI.Foundry.0.13.2\runtimes\win-x64\native\*.dll build\
```

## Run the sample

```bash
.\build\web-server-responses-vision.exe qwen3.5-0.8b
```

The sample starts the local web service, sends vision requests via the Responses API to `http://localhost:<port>/v1`, prints the model output, and then stops the web service.

## How it works

1. **Initialize** — creates the `Manager` singleton with web service configuration
2. **Model setup** — resolves the model alias, downloads if not cached, and loads into memory
3. **Web service** — starts the local Foundry web service on a random port
4. **Image encoding** — loads the image via stb, resizes to max 512px (preserving aspect ratio), and base64-encodes as JPEG
5. **Vision request** — builds the Responses API request body with `input_text` + `input_image` content parts
6. **Streaming** — sends the request via cURL with SSE streaming, printing tokens as they arrive
7. **Cleanup** — stops the web service, unloads the model, and destroys the manager

## Troubleshooting

| Error | Cause | Fix |
|---|---|---|
| `Cannot open file: test_image.jpg` | Default image not found | Ensure `test_image.jpg` is present next to the source file |
| `Model 'xyz' not found in catalog` | Invalid model alias | Check available models printed in the error output |
| `WebGPU execution provider is not supported` | WebGPUExecutionProvider not available | WebGPU models are not supported yet; the sample automatically falls back to the CPU variant |
| cURL connection refused | Web service failed to start | Ensure `config.web` is set and no port conflicts exist |

## License

Licensed under the MIT License.
