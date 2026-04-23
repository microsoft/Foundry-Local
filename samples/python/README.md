# 🚀 Foundry Local Python Samples

These samples demonstrate how to use Foundry Local with Python.

## Prerequisites

- [Python](https://www.python.org/) 3.11 or later

## Samples

| Sample | Description |
|--------|-------------|
| [native-chat-completions](native-chat-completions/) | Initialize the SDK, start the local service, and run streaming chat completions. |
| [embeddings](embeddings/) | Generate single and batch text embeddings using the Foundry Local SDK. |
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
   pip install -r requirements.txt
   ```

3. Run the sample:

   ```bash
   python src/app.py
   ```

> [!TIP]
> Each sample's `requirements.txt` uses environment markers to automatically install the right SDK for your platform. On **Windows**, `foundry-local-sdk-winml` is installed for broader hardware acceleration. On **macOS and Linux**, the standard `foundry-local-sdk` is used. Just run `pip install -r requirements.txt` — platform detection is handled for you.
