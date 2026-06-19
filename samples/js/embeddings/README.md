# Embeddings

Generates **single** and **batch** text embeddings using native in-process inference with the
Foundry Local JS SDK.

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

1. Initializes the SDK and loads the `qwen3-embedding-0.6b` embedding model.
2. Creates an embedding client.
3. Generates a **single** embedding and prints its dimensionality and first few values.
4. Generates a **batch** of embeddings for multiple inputs.
5. Unloads the model.
