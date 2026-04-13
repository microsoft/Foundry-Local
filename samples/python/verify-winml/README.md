# Verify WinML 2.0 Execution Providers

This sample verifies that WinML 2.0 execution providers are correctly discovered,
downloaded, and registered. It then runs inference on a GPU model using the WinML EP.

## Prerequisites

- Windows with a compatible GPU
- Windows App SDK 2.0 runtime installed (preview1 or experimental)
- Python 3.11+

## Setup

`requirements.txt` already adds the ORT-Nightly Python feed and combines the
public `foundry-local-sdk` package with the WinML 2.0 preview native packages,
so a plain install is enough:

```bash
pip install -r requirements.txt
```

## Run

```bash
python src/app.py
```

## What it tests

1. **EP Discovery** — Lists all available execution providers
2. **EP Download & Registration** — Downloads and registers EPs
3. **Model Catalog** — Lists GPU model variants available after EP registration
4. **Streaming Chat** — Runs streaming chat completion on a GPU model via native SDK
5. **OpenAI SDK Chat** — Runs chat completion via the OpenAI-compatible REST API
