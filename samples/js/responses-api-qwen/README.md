# Responses API with Qwen 3-0.6B

A sample app demonstrating the Foundry Local JS SDK with the `qwen3-0.6b` multimodal model. Supports text prompts and optional image input with streaming output.

## Prerequisites

- Node.js v18+
- Access to the ORT-Nightly ADO npm feed (configured via `.npmrc`)

## Setup

```bash
cd samples/js/responses-api-qwen
npm install
```

This will pull `foundry-local-sdk@1.0.0-dev.202604070007` from the ORT-Nightly feed. The `.npmrc` file in this directory configures the registry automatically.

## Usage

### Text prompt

```bash
node app.mjs "What is quantum computing?"
```

### Text + image (local file)

```bash
node app.mjs "Describe this image" --image path/to/photo.png
```

### Text + image (URL)

```bash
node app.mjs "What do you see?" --image https://example.com/image.png
```

### Check model cache

```bash
node app.mjs --check-cache
```

## How it works

1. Initializes the Foundry Local SDK and downloads execution providers
2. Looks up `qwen3-0.6b` in the catalog or cache, downloads if needed
3. Loads the model and starts the embedded web service
4. Runs inference with streaming output:
   - **Text-only** → Responses API (`/v1/responses`) via `ResponsesClient.createStreaming()`
   - **Text + image** → Chat Completions API (`/v1/chat/completions`) with streaming SSE

## Notes

- The Responses API currently has a known server-side issue with vision input (`image_grid_thw`), so image requests use the Chat Completions API as a workaround.
- The model is automatically cached after first download. Subsequent runs skip the download step.
- Supported image formats: JPEG, PNG, GIF, WebP, BMP.
