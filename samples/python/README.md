# Foundry Local Python samples

These five samples use the native `sdk_v2/python` bindings. Except for
`web-server`, inference runs in-process without an HTTP service.

| Sample | What it demonstrates |
| --- | --- |
| [`hello-world`](hello-world/) | Manager, execution providers, catalog/model lifecycle, and native chat |
| [`realtime-transcription`](realtime-transcription/) | Live microphone PCM input and native streaming transcription |
| [`embeddings`](embeddings/) | Native batched embeddings with `EmbeddingsSession` |
| [`tool-calling`](tool-calling/) | Native chat tool definitions, calls, and results |
| [`web-server`](web-server/) | Built-in server and its OpenAI-compatible endpoint |

> [!IMPORTANT]
> These samples track the latest Python SDK on `main` through an editable source dependency and require Foundry Local
> to be built before they can run. Installing
> [`foundry-local-sdk` from PyPI](https://pypi.org/project/foundry-local-sdk/) is easier, but these samples are not
> built or tested against a specific published version. See the
> [get-started documentation](https://learn.microsoft.com/en-us/azure/foundry-local/get-started) for versioned setup
> guidance and additional examples.

## Prerequisites

- Python 3.11 or later.
- A supported Foundry Local platform.
- The native sdk_v2 runtime built or installed as described in
  [`sdk_v2/python/README.md`](../../sdk_v2/python/README.md).
- Microphone access for `realtime-transcription`.

## Run a sample from the repository

Each directory has local dependency wiring to the sdk_v2 Python project:

```bash
cd samples/python/hello-world
python -m venv .venv
source .venv/bin/activate        # Windows: .venv\Scripts\activate
python -m pip install -r requirements.txt
python src/app.py
```

Use the same commands from any of the other sample directories. The first run
may download execution providers and a model. Subsequent runs reuse the local
cache.

All samples set the SDK application name to `foundry_local_samples`. Set
`FOUNDRY_LOCAL_LIB_DIR` before starting Python when using a native library from
a non-default build location.
