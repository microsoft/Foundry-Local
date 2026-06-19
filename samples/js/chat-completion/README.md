# Native Chat Completions

Runs the **same chat prompt two ways** against Foundry Local from a single app:

1. **Native in-process inference** via the SDK's chat client (non-streaming *and* streaming).
2. The **local OpenAI-compatible web server** (`/v1/chat/completions`), called with the `openai` client.

The output is split into clearly labelled sections so you can compare the two paths.

## Prerequisites

- [Foundry Local](https://github.com/microsoft/Foundry-Local) installed
- [Node.js](https://nodejs.org/) v18 or later

## Install

This sample consumes the JS SDK **directly from local source** (`sdk/js`) so it always tracks
`main` rather than a published npm version. It is **not** pinned to a registry release. The
dependency in `package.json` is:

```json
"foundry-local-sdk": "file:../../../sdk/js"
```

Install dependencies:

```bash
npm install
```

> **Building the SDK:** `npm install` resolves `foundry-local-sdk` from `sdk/js`. The SDK ships a
> prebuilt `dist/` and downloads its native runtime on install. If the local SDK has not been built
> (or you've changed its source), build it first:
>
> ```bash
> cd ../../../sdk/js
> npm install
> npm run build          # compile TypeScript -> dist/
> npm run build:native   # (re)build the native addon if needed
> ```

## Run

```bash
npm start
# or
node app.js
```

## What it does

1. Initializes the SDK (with `webServiceUrls` so the local web server has a known endpoint).
2. Discovers, downloads, and registers execution providers.
3. Downloads and loads the `qwen2.5-0.5b` model.
4. **Native inference** — runs `completeChat` and `completeStreamingChat`.
5. **Web server** — starts the local web service and sends the same prompt through the
   OpenAI-compatible `/v1/chat/completions` endpoint.
6. Unloads the model and stops the web service.
