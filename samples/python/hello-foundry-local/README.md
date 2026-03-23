# Sample: Hello Foundry Local!

This is a simple example of how to use the Foundry Local SDK to run a model locally and make requests to it. The example demonstrates how to set up the SDK, initialize a model, and make a request to the model.

## Features

- **Cache-aware**: Checks the local model cache before downloading — if the model is already cached, the download is skipped automatically.
- **Visual feedback**: Shows step-by-step status (service start → cache check → download/skip → load → ready) so you always know what's happening.

## Setup

Install the Foundry Local SDK and OpenAI packages using pip:

```bash
pip install foundry-local-sdk openai
```

> [!TIP]
> We recommend using a virtual environment to manage your Python packages using `venv` or `conda` to avoid conflicts with other packages.

## Run

```bash
python src/app.py
```
