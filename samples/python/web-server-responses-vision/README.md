# Foundry Local Python Vision Sample (Responses API, WinML 2.0)

This sample demonstrates vision (image understanding) capabilities using the Foundry Local WinML 2.0 SDK build, the local web service, and the OpenAI Responses API. By default it selects the exact `qwen3.5-9b-cuda-gpu:2` model variant.

It demonstrates:

- Downloading and registering Foundry Local execution providers
- Selecting a specific model variant by ID
- Streaming a vision response via the Responses API
- Using a default test image (`src\test_image.jpg`) if no image path is provided

## What gets installed

Install the sample dependencies from `requirements.txt`:

```powershell
py -3.13 -m pip install -r requirements.txt
```

That installs:

- `foundry-local-sdk-winml`
- `openai`
- `Pillow` (for image resizing)

The sample downloads/registers Foundry Local execution providers and downloads `qwen3.5-9b-cuda-gpu:2` the first time it runs (skips if already cached).

## Run the sample

From this directory:

```powershell
py -3.13 -m venv .venv
.\.venv\Scripts\activate
python -m pip install -r requirements.txt
python src\app.py
```

Use `py -3.11`, `py -3.12`, or `py -3.13` to match the Python version installed on your machine. Prefer `py -3.x -m pip` over bare `pip` on Windows so PATH shims do not select the wrong interpreter.

You can pass a custom prompt, image, or model reference:

```powershell
python src\app.py --prompt "Describe the image in one sentence."
python src\app.py --image C:\path\to\image.jpg --prompt "What objects are visible?"
python src\app.py --model qwen3.5-9b-cuda-gpu:2
```

On macOS or Linux, activate the virtual environment with:

```bash
source .venv/bin/activate
```

The sample starts the local web service, sends vision requests via the Responses API to `http://localhost:<port>/v1`, prints the model output, unloads the model, and then stops the web service.
