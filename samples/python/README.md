# Foundry Local Python Samples

These samples demonstrate how to use Foundry Local with Python. The Python SDK currently uses the Foundry Local CLI and the OpenAI-compatible REST API. A native in-process SDK (matching JS/C#) is coming soon.

## Prerequisites

- [Python](https://www.python.org/) 3.11 or later
- [Foundry Local CLI](../../README.md#installing) installed

## Samples

| Sample | Description |
|--------|-------------|
| [native-chat-completions](native-chat-completions/) | Initialize the SDK, start the local service, and run streaming chat completions. |
| [audio-transcription](audio-transcription/) | Transcribe audio files using the Whisper model. |
| [web-server](web-server/) | Start a local OpenAI-compatible web server and call it with the OpenAI Python SDK. |
| [tool-calling](tool-calling/) | Tool calling with custom function definitions (get_weather, calculate). |
| [langchain-integration](langchain-integration/) | LangChain integration for building translation and text generation chains. |
| [tutorial-chat-assistant](tutorial-chat-assistant/) | Build an interactive multi-turn chat assistant (tutorial). |
| [tutorial-document-summarizer](tutorial-document-summarizer/) | Summarize documents with AI (tutorial). |
| [tutorial-tool-calling](tutorial-tool-calling/) | Create a tool-calling assistant (tutorial). |
| [tutorial-voice-to-text](tutorial-voice-to-text/) | Transcribe and summarize audio (tutorial). |

## Running a Sample

1. Clone the repository:

   ```bash
   git clone https://github.com/microsoft/Foundry-Local.git
   cd Foundry-Local/samples/python
   ```

2. Navigate to a sample and install dependencies:

   ```bash
   cd native-chat-completions
   pip install foundry-local-sdk
   ```

3. Run the sample:

   ```bash
   python src/app.py
   ```

> [!TIP]
> Some samples require additional packages (e.g., `openai`, `langchain-openai`). Check for a `requirements.txt` or the import statements at the top of the source file.
