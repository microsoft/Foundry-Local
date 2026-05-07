# Foundry Local C++ Vision Sample (Responses API)

This sample demonstrates vision (image understanding) capabilities using the Foundry Local web service and the OpenAI Responses API.

> **Windows-only** — requires MSVC or clang-cl (MSVC-compatible toolchain).

## Features

- **Vision inference** — send an image to a vision-capable model and get a description
- **Streaming** — token-by-token output via Server-Sent Events (SSE)
- **Responses API** — uses the `/v1/responses` endpoint (not chat completions)
- Uses a default test image (`test_image.jpg`)

## Prerequisites

| Requirement | Notes |
|---|---|
| **CMake >= 3.20** | Ships with Visual Studio 2022 |
| **Ninja** | Ships with Visual Studio 2022 |
| **vcpkg** | Set the `VCPKG_ROOT` environment variable to your vcpkg installation |
| **MSVC** (or clang-cl) | Visual Studio 2022 Build Tools or full IDE |
| **NuGet CLI** | Must be on PATH. Install from [nuget.org/downloads](https://www.nuget.org/downloads) |

The sample downloads the specified model the first time it runs (skips if already cached).

## Build

Open an **x64 Native Tools Command Prompt for VS 2022** (or run `vcvars64.bat`), then navigate to the sample directory:

```bash
cd sdk/cpp/sample/web-server-responses-vision
```

### Configure (CMake + vcpkg)

```bash
cmake --preset x64-debug
```

### Build

```bash
cmake --build --preset x64-debug
```

CMake will automatically:
- Install vcpkg dependencies (`nlohmann-json`, `ms-gsl`, `curl`, `stb`)
- Download the required NuGet packages (`Microsoft.AI.Foundry.Local.Core`, `Microsoft.ML.OnnxRuntime.Foundry`, `Microsoft.ML.OnnxRuntimeGenAI.Foundry`)
- Copy runtime DLLs next to the executable after build

The built executable will be at `out/build/x64-debug/web-server-responses-vision.exe`.

## Run the sample

```bash
.\out\build\x64-debug\web-server-responses-vision.exe qwen3.5-0.8b
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
| `Failed to load image: <path>` | Default image not found | Ensure `test_image.jpg` is present next to the source file |
| `Model 'xyz' not found in catalog` | Invalid model alias | Check available models printed in the error output |
| `WebGPU execution provider is not supported` | WebGPUExecutionProvider not available | WebGPU models are not supported yet; the sample automatically falls back to the CPU variant |
| cURL connection refused | Web service failed to start | Ensure `config.web` is set and no port conflicts exist |

## License

Licensed under the MIT License.
