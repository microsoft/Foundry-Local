# Web Server Responses — Vision Example

Streams a **vision (image understanding)** response from the Foundry Local **local web server**
using the OpenAI-compatible **Responses API** (`/v1/responses`). A bundled `test_image.jpg` is
used by default.

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
# Use a vision model alias with the bundled test image
node app.js qwen3.5-0.8b

# Use a specific image
node app.js qwen3.5-0.8b ./my-image.jpg

# Use a specific variant id
node app.js Qwen2.5-VL-7B-Instruct-generic-cpu

# List vision models (and variants) available in the catalog
node app.js --list-models
```

## What it does

1. Initializes the SDK (with `webServiceUrls` so the local web server has a known endpoint).
2. Downloads and registers execution providers.
3. Resolves the requested vision model (by alias or variant id), downloads, and loads it.
4. Starts the local web service.
5. Base64-encodes the image and POSTs it to `/v1/responses` with `input_text` + `input_image`
   content parts, streaming the assistant's description back via Server-Sent Events.
6. Unloads the model and stops the web service.
