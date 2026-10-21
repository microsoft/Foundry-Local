# Foundry Local C++ samples

These five samples use the Foundry Local SDK v2 public C++ API:

| Sample | Demonstrates |
|---|---|
| `hello-world` | Complete manager, execution-provider, catalog, model, session, and cleanup lifecycle |
| `realtime-transcription` | Native streaming audio transcription from a microphone or synthetic PCM |
| `embeddings` | Native `EmbeddingsSession` inference with `qwen3-embedding-0.6b` |
| `tool-calling` | Native `ChatSession`, tool-call items, and tool-result items |
| `web-server` | Built-in server lifecycle and an OpenAI-compatible HTTP request |

The non-web samples run inference directly in process. The web-server sample is intentionally the only sample that
routes inference through HTTP.

> [!IMPORTANT]
> These samples track the latest C++ SDK on `main` and require a local Foundry Local build. The published
> [Runtime NuGet package](https://www.nuget.org/packages/Microsoft.AI.Foundry.Local.Runtime) is easier to consume, but
> these samples are not built or tested against a specific package version. See the
> [get-started documentation](https://learn.microsoft.com/en-us/azure/foundry-local/get-started) for versioned setup
> guidance and additional examples.

## Prerequisites

- CMake 3.20 or newer
- A C++20 compiler
- Python 3
- The platform prerequisites listed in [`sdk_v2/cpp/README.md`](../../sdk_v2/cpp/README.md)
- Optional: PortAudio development files for live microphone capture

## Build

Build the SDK first through its supported build driver. Do not build the SDK directly with CMake:

```sh
cd sdk_v2/cpp
python build.py --configure --build --config RelWithDebInfo
cd ../../samples/cpp
```

Configure and build the samples:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
```

The sample project finds the canonical SDK output at
`sdk_v2/cpp/build/<Windows|Linux|macOS>/RelWithDebInfo`. For a different canonical configuration, pass its directory:

```sh
cmake -S . -B build -DFOUNDRY_LOCAL_SDK_BUILD_DIR=/path/to/sdk_v2/cpp/build/<platform>/<config>
```

On Linux or macOS, ensure the SDK shared library and its adjacent runtime dependencies are in the loader search path.
On Windows, run from a directory containing the SDK DLL and its runtime dependencies, or add that directory to `PATH`.

## Run

Executable locations depend on the CMake generator. With a single-config generator:

```sh
./build/hello-world
./build/realtime-transcription
./build/realtime-transcription --synthetic
./build/embeddings
./build/tool-calling
./build/web-server
```

When PortAudio is available at configure time, `realtime-transcription` records five seconds from the default
microphone. Pass `--synthetic` to force the fallback. Without PortAudio, the sample automatically streams a generated
16 kHz mono PCM signal; because it contains no speech, the resulting transcript may be empty.

The first run may download execution providers and model assets. Later runs reuse the application cache associated
with the exact app name `foundry_local_samples`.
