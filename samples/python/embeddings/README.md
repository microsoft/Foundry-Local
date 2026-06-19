# Embeddings Example

Generate single and batch text embeddings with the Foundry Local Python SDK.

## Prerequisites

- [Foundry Local](https://github.com/microsoft/Foundry-Local) installed
- Python 3.11+

## Setup

This sample installs the Foundry Local SDK **from local repo source** (an
editable install of `sdk/python`), so it always tracks `main` rather than a
published PyPI release:

```bash
cd samples/python/embeddings
pip install -r requirements.txt
```

That installs:

- `foundry-local-sdk` (editable, from `../../../sdk/python`)

## Run

```bash
python src/app.py
```

## How it works

1. Initializes the SDK and loads the `qwen3-embedding-0.6b` model.
2. Generates a single embedding and prints its dimensions and first values.
3. Generates a batch of embeddings and prints the dimensions of each.
4. Unloads the model.
