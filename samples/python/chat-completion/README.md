# Chat Completions Example (Native + Web Server)

Run the **same chat prompt two ways** with the Foundry Local Python SDK:

1. **Native inference** — in-process streaming chat completions through the SDK
   chat client (`model.get_chat_client()`), no web server involved.
2. **Web server** — the local OpenAI-compatible REST endpoint
   (`/v1/chat/completions`) called with the official `openai` Python client.

The program prints clear section headers (`=== Native inference ===` and
`=== Web server (/v1/chat/completions) ===`) so you can compare the two paths.

## Prerequisites

- [Foundry Local](https://github.com/microsoft/Foundry-Local) installed
- Python 3.11+

## Setup

This sample installs the Foundry Local SDK **from local repo source** (an
editable install of `sdk/python`), so it always tracks `main` rather than a
published PyPI release:

```bash
cd samples/python/chat-completion
pip install -r requirements.txt
```

That installs:

- `foundry-local-sdk` (editable, from `../../../sdk/python`)
- `openai` (for the local web-server flow)

## Run

```bash
python src/app.py
```

You will see the model download and load once, then the same prompt answered
first by native inference and then by the local web server.

## How it works

1. Initializes the SDK and registers execution providers.
2. Downloads and loads `qwen2.5-0.5b` from the catalog.
3. Streams a response with the in-process chat client (native inference).
4. Starts the local web service and sends the same messages through the
   `openai` client against `http://localhost:<port>/v1`.
5. Stops the web service and unloads the model.
