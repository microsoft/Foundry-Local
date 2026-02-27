# Foundry Local Chat - Electron Application

A modern, full-featured chat application built with Electron and the Foundry Local SDK. Chat with AI models running entirely on your local machine with complete privacy.

![Foundry Local Chat](https://img.shields.io/badge/Electron-34.1.0-47848F?logo=electron)
![Node.js](https://img.shields.io/badge/Node.js-18+-339933?logo=node.js)

## Features

### Core Features
- **🔒 100% Private** - All AI inference runs locally on your machine
- **⚡ Low Latency** - Direct local inference with no network round trips
- **📊 Performance Metrics** - Real-time tokens/second and time-to-first-token stats
- **🎨 Modern UI** - Beautiful dark theme with smooth animations
- **💬 Markdown Support** - Code blocks with syntax highlighting, headings, and lists
- **📋 Copy Code** - One-click copy button on all code blocks

### Model Management
- **📦 Download Models** - Browse and download models from the catalog
- **🔄 Load/Unload** - Easily switch between downloaded models
- **🗑️ Delete Models** - Remove downloaded models to free up disk space
- **🟢 Visual Status** - Green background for loaded model, green dot for downloaded

### Voice Transcription
- **🎤 Voice Input** - Record voice messages with the microphone button
- **🗣️ Whisper Integration** - Uses OpenAI Whisper models for accurate transcription
- **⚙️ Transcription Settings** - Choose from multiple Whisper model sizes
- **🔊 Audio Processing** - Automatic conversion to 16kHz WAV for optimal quality

### Context Tracking
- **📏 Context Usage** - Visual progress bar showing how much context is used
- **⚠️ Usage Warnings** - Bar changes color (green → yellow → red) as context fills

## Screenshots

Here is a screenshot of the chat interface with some annotations highlighting key features:

![Chat Interface](./screenshots/electron-description-of-functions.png)

*On the first use* of the microphone button, you will be prompted to download a Whisper model for transcription:

![Whisper Transcription](./screenshots/electron-transcription.png)

You can also change and/or delete the model for transcription using the *Voice settings* link just underneath the text input box.

## Prerequisites

- [Node.js](https://nodejs.org/) 18 or later

## Installation

To set up and run the Electron Chat Application, follow these steps:

1. Navigate to the sample directory:
    ```bash
    cd samples/js/electron-chat-application
    ```

1. Install dependencies:
    ```bash
    npm install --registry https://registry.npmjs.org
    ```

1. Install the Foundry Local SDK:

    **macOS / Linux:**
    ```bash
    npm install --foreground-scripts foundry-local-sdk@0.9.0-rc2
    ```

    **Windows:**
    ```bash
    npm install --foreground-scripts --winml foundry-local-sdk@0.9.0-rc2
    ```

1. **Workaround for macOS / Linux:**

    > **Note:** There is a known issue where ONNX Runtime is not picked up on macOS / Linux. This will be fixed in ORT 1.24.3. In the meantime, add the ONNX Runtime native library to your library path before running the app:
    >
    > **macOS:**
    > ```bash
    > export DYLD_LIBRARY_PATH=$DYLD_LIBRARY_PATH:$(pwd)/node_modules/@foundry-local-core/darwin-arm64
    > ```
    >
    > **Linux:**
    > ```bash
    > export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/node_modules/@foundry-local-core/linux-x64
    > ```
    >
    > Run this from the sample directory after installing dependencies. The platform-specific path (e.g., `darwin-arm64`, `linux-x64`) will vary depending on your system architecture.

1. Start the application:
    ```bash
    npm start
    ```

## Usage

### Basic Chat
1. **Start the app** - Run `npm start` to launch the Electron application
2. **Download a model** - Click "Download" on any available model
3. **Load the model** - Click "Load" on a downloaded model (background turns green when loaded)
4. **Start chatting** - Type your message and press Enter to send
5. **View stats** - Each AI response shows TTFT and tokens/sec metrics

### Voice Transcription
1. **Click the microphone** - Opens Whisper model selection if first time
2. **Download a Whisper model** - Choose a size (tiny is fastest, large is most accurate)
3. **Record your voice** - Click mic to start, click stop when done
4. **Auto-transcription** - Text appears in the input field automatically

### Model Management
- **Load**: Click "Load" button on any downloaded model
- **Unload**: Click "Unload" on the currently loaded model
- **Delete**: Click the trash icon to remove a downloaded model from cache

## Project Structure

```
electron-chat-application/
├── main.js              # Electron main process - SDK integration & IPC handlers
├── preload.js           # Secure bridge between main and renderer
├── index.html           # Main application UI
├── styles.css           # Modern dark theme CSS
├── renderer.js          # Chat UI logic, markdown rendering, voice recording
├── foundry_local_color.svg  # Application logo
├── package.json         # Dependencies and scripts
└── README.md            # This file
```

## Architecture

### Main Process (`main.js`)
- Initializes Foundry Local SDK with HTTP web service
- Handles model loading/unloading via IPC
- Streams chat completions using Server-Sent Events (SSE)
- Manages audio transcription with Whisper models

### Preload Script (`preload.js`)
- Exposes secure API to renderer via `contextBridge`
- Handles IPC communication for all SDK operations

### Renderer Process (`renderer.js`)
- Manages chat UI and message display
- Implements SimpleMarkdown parser for rich text
- Handles voice recording and WAV conversion
- Tracks context usage and updates UI

## API Reference

The renderer has access to the Foundry Local SDK via `window.foundryAPI`. This bridge is exposed via the preload script using Electron's `contextBridge`, allowing secure communication between the renderer and main process while maintaining `contextIsolation`. Each method invokes IPC handlers in the main process that call the underlying Foundry Local SDK to manage models and perform inference.

### Available Methods

| Method | Purpose | SDK Operation |
|--------|---------|---------------|
| `getModels()` | Fetches available AI models from the Foundry Local catalog | `manager.catalog.getModels()` |
| `downloadModel(alias)` | Downloads a model to local cache | `model.download()` |
| `loadModel(alias)` | Loads a model into memory for inference | `model.load()` |
| `unloadModel()` | Unloads the currently loaded model | `model.unload()` |
| `deleteModel(alias)` | Removes a model from local cache | `model.removeFromCache()` |
| `chat(messages)` | Sends chat messages to the loaded model and returns response | HTTP streaming via SDK web service |
| `getLoadedModel()` | Returns info about the currently loaded model | Returns cached model state |
| `onChatChunk(callback)` | Subscribes to streaming chat response chunks (returns cleanup function) | IPC event listener |
| `getWhisperModels()` | Lists available Whisper models for transcription | `manager.catalog.getModels()` (filtered) |
| `downloadWhisperModel(alias)` | Downloads a Whisper model | `model.download()` |
| `transcribeAudio(path, base64)` | Transcribes audio using Whisper | `audioClient.transcribe()` |

### Usage Examples

```javascript
// Get all available models
const models = await foundryAPI.getModels();

// Download a model
await foundryAPI.downloadModel('phi-4');

// Load a model for chat
await foundryAPI.loadModel('phi-4');

// Unload the current model
await foundryAPI.unloadModel();

// Delete a model from cache
await foundryAPI.deleteModel('phi-4');

// Send chat messages (streaming)
const response = await foundryAPI.chat([
  { role: 'user', content: 'Hello!' }
]);

// Listen for streaming chunks
foundryAPI.onChatChunk((data) => {
  console.log(data.content, data.tokenCount);
});

// Get Whisper models for transcription
const whisperModels = await foundryAPI.getWhisperModels();

// Download a Whisper model
await foundryAPI.downloadWhisperModel('whisper-small');

// Transcribe audio (base64 WAV data)
const text = await foundryAPI.transcribeAudio(base64WavData);
```

## Customization

### Theming
Edit CSS variables in `styles.css`:
```css
:root {
  --accent-primary: #6366f1;    /* Primary accent color */
  --accent-secondary: #818cf8;  /* Secondary accent */
  --success: #10b981;           /* Success/loaded state */
  --warning: #f59e0b;           /* Warning state */
  --error: #ef4444;             /* Error state */
  --bg-primary: #0f0f1a;        /* Main background */
}
```

### Context Window
Adjust the context limit in `renderer.js`:
```javascript
const CONTEXT_LIMIT = 8192; // Default context window size
```

### Sidebar Width
The sidebar is resizable between 240-480px. Default is 320px, configured in CSS:
```css
.sidebar {
  width: 320px;
  min-width: 240px;
  max-width: 480px;
}
```

## Technical Notes

### HTTP Streaming
The app uses HTTP streaming via the SDK's built-in web service (port 47392) instead of native callbacks, which provides better compatibility with Electron's process model.

### Audio Processing
Voice recordings are converted to 16kHz mono 16-bit PCM WAV format before transcription, as required by Whisper models. The conversion uses Web Audio API's OfflineAudioContext for resampling.

### Temporary Files
Audio files are stored in the system temp directory (`os.tmpdir()`) and automatically cleaned up after transcription.

## Troubleshooting

**Slow performance?**
- Try a smaller model variant (e.g., phi-4-mini instead of phi-4)

**Transcription not working?**
- Ensure you've downloaded a Whisper model first
- Check microphone permissions in System Preferences
- Verify audio is recording (mic icon changes to stop icon)

**High context usage?**
- Click "New Chat" to clear the conversation and reset context
- The context bar shows usage: green (<70%), yellow (70-90%), red (>90%)

## License

MIT

