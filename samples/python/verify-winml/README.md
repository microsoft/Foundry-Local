# Verify WinML 2.0 Execution Providers

This sample verifies that WinML 2.0 execution providers are correctly discovered,
downloaded, and registered. It then runs inference on a model variant backed by a
registered WinML EP. It finishes with one native streaming chat check.

## Prerequisites

- Windows with a compatible GPU
- Windows App SDK 2.0 runtime installed (preview1 or experimental)
- Python 3.11+

## Setup

Use a fresh virtual environment for this sample.

`requirements.txt` already adds the ORT-Nightly Python feed and combines the
public `foundry-local-sdk` package with the WinML 2.0 preview native packages,
so a plain install is enough:

```bash
python -m venv .venv
.venv\Scripts\Activate.ps1
pip install -r requirements.txt
```

## Run

```bash
python src/app.py
```

## What it tests

1. **EP Discovery** — Lists all available execution providers
2. **EP Download & Registration** — Downloads only the WinML EPs relevant to the machine
3. **Model Catalog** — Lists model variants backed by the registered WinML EPs
4. **Streaming Chat** — Runs streaming chat completion on a WinML EP-backed model via native SDK
