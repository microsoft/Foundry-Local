# 🚀 Foundry Local Python Samples

These samples demonstrate how to use Foundry Local with Python.

They consume the SDK **from local repo source** (an editable install of
`sdk/python`), so they always track `main` rather than a published PyPI release.

## Prerequisites

- [Python](https://www.python.org/) 3.11 or later

## Samples

| Sample | Description |
|--------|-------------|
| [chat-completion](chat-completion/) | Run the same chat prompt two ways: native in-process inference **and** the local OpenAI-compatible web server (`/v1/chat/completions`). |
| [embeddings](embeddings/) | Generate single and batch text embeddings using the Foundry Local SDK. |
| [audio](audio/) | Transcribe audio two ways: live microphone streaming with Nemotron ASR (default) **and** file-based transcription with Whisper via `--file`. |
| [responses-api](responses-api/) | Vision (image understanding) via the local web server using the OpenAI Responses API. |

## Running a Sample

1. Clone the repository:

   ```bash
   git clone https://github.com/microsoft/Foundry-Local.git
   cd Foundry-Local/samples/python
   ```

2. Navigate to a sample and install dependencies (this installs the SDK from
   `sdk/python` source via an editable install):

   ```bash
   cd chat-completion
   pip install -r requirements.txt
   ```

3. Run the sample:

   ```bash
   python src/app.py
   ```

> [!TIP]
> Each sample's `requirements.txt` installs the base SDK from local source with
> `-e ../../../sdk/python`, so the samples track `main` and are **not**
> version-pinned to PyPI.
