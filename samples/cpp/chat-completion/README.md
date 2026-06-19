# Chat Completion (C++)

Runs the **same chat prompt** through the Foundry Local C++ SDK (`sdk_v2/cpp`) in
three ways so you can see the two execution surfaces side by side:

1. **Native, in-process (non-streaming)** — `ChatSession::ProcessRequest`.
2. **Native, in-process (streaming)** — incremental tokens via a streaming callback.
3. **Local web server** — host the embedded OpenAI-compatible service with
   `AddWebServiceEndpoint` + `StartWebService`, then `POST /v1/chat/completions`
   over loopback using a tiny built-in HTTP client (no third-party HTTP dependency).

The same loaded model backs all three paths — the web service reuses the model the
SDK already loaded in-process.

This sample tracks **`main`** — it builds against your **local** `sdk_v2/cpp` build,
not a pinned SDK release.

## What it does

1. Creates a `Manager` with an embedded web service endpoint
   (`http://127.0.0.1:0` — an ephemeral port chosen by the OS).
2. Resolves the `qwen2.5-0.5b` chat model, downloading + loading it if needed.
3. Runs the prompt natively (non-streaming, then streaming).
4. Starts the web service, discovers the bound URL via `GetWebServiceEndpoints()`,
   and POSTs the same prompt to `/v1/chat/completions` (the request body is built
   from typed structs serialized with `nlohmann/json`).

> The web service resolves models by their full **variant id** (e.g.
> `qwen2.5-0.5b-instruct-generic-cpu`), which the sample reads from
> `ModelInfo::Id()` — not the short alias.

## Prerequisites

```bash
python ../../../sdk_v2/cpp/build.py
```

## Build

```bash
cmake -S . -B build
cmake --build build
```

Override the SDK config/location if needed:
`-DFOUNDRY_LOCAL_BUILD_CONFIG=Debug`, `-DFOUNDRY_LOCAL_SDK_DIR=...`,
`-DFOUNDRY_LOCAL_BUILD_DIR=...`.

## Run

```bash
./build/chat_completion     # Windows: .\build\chat_completion.exe
```

The first run downloads the model; later runs use the cache.
