# Responses API with Qwen 3.5

A sample app demonstrating the Foundry Local Python SDK Responses API with the Qwen 3.5 multimodal model. Supports text prompts and optional image input with streaming output.

The default model is `qwen3.5-4b`. The following Qwen 3.5 variants are also supported:

| Model | Alias |
|-------|-------|
| Qwen 3.5 0.8B | `qwen3.5-0.8b` |
| Qwen 3.5 2B | `qwen3.5-2b` |
| Qwen 3.5 4B | `qwen3.5-4b` |
| Qwen 3.5 9B | `qwen3.5-9b` |

To use a different variant, update the `MODEL_ALIAS` constant in `src/app.py`.

## Prerequisites

- [Python](https://www.python.org/) 3.11 or later

## Setup

```bash
cd samples/python/responses-api
pip install -r requirements.txt
```

## Usage

### Text prompt

```bash
python src/app.py "What is quantum computing?"
```

### Text + image (local file)

```bash
python src/app.py "Describe this image" --image path/to/photo.png
```

### Text + image (URL)

```bash
python src/app.py "What do you see?" --image "https://example.com/photo.jpg"
```

### Check model cache

```bash
python src/app.py --check-cache
```

## How it works

1. Initializes the Foundry Local SDK and downloads execution providers
2. Looks up `qwen3.5-4b` in the catalog or cache, downloads if needed
3. Selects the CPU variant if multiple variants exist
4. Loads the model and starts the embedded web service
5. Runs inference with streaming output via the Responses API (`/v1/responses`)
   - **Text-only** — sends an `input_text` content part
   - **Text + image** — sends `input_image` + `input_text` content parts

## Notes

- Image support requires `Pillow` (installed via `requirements.txt`) for resizing large images.
- Images larger than 480px on either dimension are automatically resized to avoid OGA errors.
- The model is automatically cached after first download. Subsequent runs skip the download step.
- Supported image formats: JPEG, PNG, GIF, WebP, BMP.
