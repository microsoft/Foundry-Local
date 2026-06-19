# Embeddings (C++)

Generates text embeddings **natively, in-process** with the Foundry Local C++ SDK
(`sdk_v2/cpp`) — no web server involved. It embeds a single sentence, then a batch
of sentences, and prints the cosine similarity between every pair so you can see
that semantically related sentences score higher.

This sample tracks **`main`** — it builds against your **local** `sdk_v2/cpp` build,
not a pinned SDK release.

## What it does

1. Creates a `Manager` and finds the first `embeddings` model in the catalog
   (e.g. `qwen3-embedding-0.6b`).
2. Downloads the model if it isn't cached, then loads it.
3. Uses an `EmbeddingsSession` to:
   - embed a single string and print its dimensionality + first few values;
   - embed a batch of strings and print pairwise cosine similarities.

## Prerequisites

Build the SDK once so the shared library and headers exist:

```bash
python ../../../sdk_v2/cpp/build.py
```

This produces `sdk_v2/cpp/build/<macOS|Linux|Windows>/RelWithDebInfo/`, which the
sample's CMake locates automatically.

## Build

```bash
cmake -S . -B build
cmake --build build
```

If you built the SDK with a different configuration, pass it through:

```bash
cmake -S . -B build -DFOUNDRY_LOCAL_BUILD_CONFIG=Debug
```

You can also point at a non-default SDK location with
`-DFOUNDRY_LOCAL_SDK_DIR=<path-to>/sdk_v2/cpp` or
`-DFOUNDRY_LOCAL_BUILD_DIR=<path-to-build-output>`.

## Run

```bash
./build/embeddings          # Windows: .\build\embeddings.exe
```

The first run downloads the embeddings model (a few hundred MB); subsequent runs
use the cache.
