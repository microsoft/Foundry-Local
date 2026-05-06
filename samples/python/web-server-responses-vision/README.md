# Foundry Local Python Vision Sample (Responses API)

This sample demonstrates vision (image understanding) capabilities using the Foundry Local web service and the OpenAI Responses API.

It demonstrates:

- Streaming a vision response with a local image via the Responses API
- Streaming a text-only response (when no image is provided)

## What gets installed

Install the sample dependencies from `requirements.txt`:

```bash
pip install -r requirements.txt
```

That installs:

- `foundry-local-sdk`
- `openai`
- `Pillow` (for image resizing)

The sample downloads the specified model the first time it runs (skips if already cached).

## Run the sample

From this directory:

```bash
python -m venv .venv
.\.venv\Scripts\activate
pip install -r requirements.txt
python src\app.py <model_alias> [image_path]
```

Examples:

```bash
# Vision with an image
python src\app.py qwen3.5-0.8b path\to\image.jpg

# Text only
python src\app.py qwen3.5-0.8b
```

On macOS or Linux, activate the virtual environment with:

```bash
source .venv/bin/activate
```

The sample starts the local web service, sends vision requests via the Responses API to `http://localhost:<port>/v1`, prints the model output, and then stops the web service.
