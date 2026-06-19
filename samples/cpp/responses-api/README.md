# Responses API — Vision (C++)

Demonstrates **image understanding** with the Foundry Local C++ SDK (`sdk_v2/cpp`)
through the OpenAI-compatible **Responses API** (`POST /v1/responses`).

The Responses API is exposed by the embedded **web service**, so this sample hosts
that service in-process with `AddWebServiceEndpoint` + `StartWebService`, then sends
an image + prompt over loopback HTTP and prints the model's description.

This sample tracks **`main`** — it builds against your **local** `sdk_v2/cpp` build,
not a pinned SDK release.

## What it does

1. Creates a `Manager` with an embedded web service endpoint
   (`http://127.0.0.1:0` — an ephemeral port).
2. Resolves a vision-capable model (default: `qwen3.5-0.8b`), downloading +
   loading it if needed.
3. Starts the web service and discovers the bound URL via `GetWebServiceEndpoints()`.
4. Base64-encodes a bundled image into a `data:image/jpeg;base64,...` URL.
5. POSTs a Responses API request whose message has an `input_text` part and an
   `input_image` part, then prints the response's `output_text`.

A small default image (`test_image.jpg`, 256×256) ships with the sample so it runs
out of the box.

> **Image input format.** The sdk_v2 Responses API expects `input_image.image_url`
> to be either a `data:` URL (used here) or a local file path — remote `http(s)`
> image URLs are not supported. This differs from the v1 sample, which used a
> separate `image_data` + `media_type` shape.
>
> **Model id vs alias.** The web service resolves models by their full **variant
> id** (from `ModelInfo::Id()`), not the short alias.

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
# Default vision model + bundled image:
./build/responses_api                          # Windows: .\build\responses_api.exe

# Custom vision model:
./build/responses_api qwen3.5-0.8b

# Custom model + custom image:
./build/responses_api qwen3.5-0.8b /path/to/image.jpg
```

The first run downloads the model; later runs use the cache.
