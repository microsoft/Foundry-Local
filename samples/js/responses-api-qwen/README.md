# Responses API with Qwen 3-0.6B

A sample app demonstrating the Foundry Local JS SDK Responses API with the `qwen3-0.6b` model. Supports text prompts and optional image input with streaming output.

## Prerequisites

- [Node.js](https://nodejs.org/) v20 or later
- Access to the ORT-Nightly ADO npm feed (configured via `.npmrc`)

## Setup

```bash
cd samples/js/responses-api-qwen
npm init -y
npm pkg set type=module
npm install foundry-local-sdk@1.0.0-dev.202604072255
```

The `.npmrc` file in this directory configures the ORT-Nightly ADO registry automatically.

## Usage

### Text prompt

```bash
node app.js "What is quantum computing?"
```

### Text + image (local file)

```bash
node app.js "Describe this image" --image path/to/photo.png
```

### Text + image (URL)

```bash
node app.js "What do you see?" --image https://example.com/image.png
```

### Check model cache

```bash
node app.js --check-cache
```

## How it works

1. Initializes the Foundry Local SDK and downloads execution providers
2. Looks up `qwen3-0.6b` in the catalog or cache, downloads if needed
3. Loads the model and starts the embedded web service
4. Runs inference with streaming output via the Responses API (`/v1/responses`)
   - **Text-only** → `ResponsesClient.createStreaming()`
   - **Text + image** → `ResponsesClient.create()` with `input_image` content part

## Notes

- Image support requires ORT-GenAI v0.13.0+ for vision models with `image_grid_thw` support.
- The model is automatically cached after first download. Subsequent runs skip the download step.
- Supported image formats: JPEG, PNG, GIF, WebP, BMP.
