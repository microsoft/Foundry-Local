# Foundry Local — C++ Samples

Self-contained C++ samples for the **`sdk_v2/cpp`** SDK (the C++ rewrite, public
header `foundry_local/foundry_local_cpp.h`).

These samples track **`main`**: they build against your **local** `sdk_v2/cpp`
build, not a pinned SDK release.

## Build the SDK first

Every sample links the locally-built SDK shared library, so build it once:

```bash
python ../../sdk_v2/cpp/build.py
```

This produces `sdk_v2/cpp/build/<platform>/<config>/` (default config
`RelWithDebInfo`). The shared `cmake/FoundryLocalSDK.cmake` module locates that
output automatically; override it with `-DFOUNDRY_LOCAL_BUILD_CONFIG=...`,
`-DFOUNDRY_LOCAL_SDK_DIR=...`, or `-DFOUNDRY_LOCAL_BUILD_DIR=...` if needed.

## Samples

| Sample                              | What it shows                                                                 |
|-------------------------------------|-------------------------------------------------------------------------------|
| [`chat-completion`](chat-completion)| One chat prompt, run natively in-process **and** over the local web server (`POST /v1/chat/completions`), including streaming. |
| [`embeddings`](embeddings)          | Native single and batch text embeddings.                                       |
| [`audio`](audio)                    | Streaming ASR transcription from live mic (optional PortAudio) or a WAV file.   |
| [`responses-api`](responses-api)    | Vision / image understanding over the local web server (`POST /v1/responses`).  |

## Build and run a sample

Each sample is standalone:

```bash
cd chat-completion          # or embeddings, audio, responses-api
cmake -S . -B build
cmake --build build
./build/<target>            # see the sample's README for the exact target/args
```

Shared, header-only helpers (`common/`) and the build-wiring module (`cmake/`) are
reused across samples.
