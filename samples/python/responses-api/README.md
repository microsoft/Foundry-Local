# Foundry Local Python Vision Sample (Responses API)

This sample demonstrates vision (image understanding) capabilities using the Foundry Local web service and the OpenAI Responses API.

It demonstrates:

- Streaming a vision response via the Responses API
- Uses a default test image (`src/test_image.jpg`) if no image path is provided

## What gets installed

This sample installs the Foundry Local SDK **from local repo source** (an
editable install of `sdk/python`), so it always tracks `main` rather than a
published PyPI release:

```bash
pip install -r requirements.txt
```

That installs:

- `foundry-local-sdk` (editable, from `../../../sdk/python`)
- `openai`
- `Pillow` (for image handling)

The sample downloads the specified model the first time it runs (skips if already cached).

## Run the sample

From this directory:

```bash
cd samples/python/responses-api
python -m venv .venv
.\.venv\Scripts\activate
pip install -r requirements.txt
python src\app.py qwen3.5-0.8b
```

You can also pass a custom image path as the second argument.

On macOS or Linux, activate the virtual environment with:

```bash
source .venv/bin/activate
```

The sample starts the local web service, sends vision requests via the Responses API to `http://localhost:<port>/v1`, prints the model output, and then stops the web service.
