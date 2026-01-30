# Foundry Local Chat - Electron Application

A modern, full-featured chat application built with Electron and the Foundry Local SDK. Chat with AI models running entirely on your local machine with complete privacy.

![Foundry Local Chat](https://img.shields.io/badge/Electron-34.1.0-47848F?logo=electron)
![Node.js](https://img.shields.io/badge/Node.js-18+-339933?logo=node.js)

## Features

- **🔒 100% Private** - All AI inference runs locally on your machine
- **⚡ Low Latency** - Direct local inference with no network round trips
- **📊 Performance Metrics** - Real-time tokens/second and time-to-first-token stats
- **🎨 Modern UI** - Beautiful dark theme with smooth animations
- **💬 Markdown Support** - Code blocks with syntax highlighting and copy buttons
- **📦 Model Management** - Download and switch between models from the sidebar

## Screenshots

The app features a clean, modern interface with:
- Collapsible sidebar showing available models
- Downloaded models shown first with green indicator
- Chat area with message bubbles
- Performance stats (TTFT, tokens/sec) on each response
- Code blocks with copy functionality

## Prerequisites

- [Node.js](https://nodejs.org/) 18 or later
- [Foundry Local](https://github.com/microsoft/Foundry-Local) installed and running

## Installation

```bash
# Navigate to the sample directory
cd samples/js/electron-chat-application

# Install dependencies
npm install

# Start the application
npm start
```

## Usage

1. **Start the app** - Run `npm start` to launch the Electron application
2. **Select a model** - Click on a downloaded model in the sidebar (green dot = downloaded)
3. **Download new models** - Click "Download" on any available model to download it
4. **Start chatting** - Type your message and press Enter to send
5. **View stats** - Each AI response shows performance metrics

## Project Structure

```
electron-chat-application/
├── main.js        # Electron main process - SDK integration & IPC handlers
├── preload.js     # Secure bridge between main and renderer
├── index.html     # Main application UI
├── styles.css     # Modern dark theme CSS
├── renderer.js    # Chat UI logic and markdown rendering
├── package.json   # Dependencies and scripts
└── README.md      # This file
```

## Architecture

- **Main Process** (`main.js`): Handles the Foundry Local SDK initialization, model management, and chat completions via IPC handlers
- **Preload Script** (`preload.js`): Exposes a secure API to the renderer via `contextBridge`
- **Renderer Process** (`renderer.js`): Manages the UI, message display, and user interactions

## API Reference

The renderer has access to these APIs via `window.foundryAPI`:

```javascript
// Get all available models
const models = await foundryAPI.getModels();

// Download a model
await foundryAPI.downloadModel('model-alias');

// Load a model for chat
await foundryAPI.loadModel('model-alias');

// Send chat messages
const response = await foundryAPI.chat([
  { role: 'user', content: 'Hello!' }
]);

// Listen for streaming chunks
foundryAPI.onChatChunk((data) => {
  console.log(data.content);
});
```

## Customization

### Theming
Edit CSS variables in `styles.css`:
```css
:root {
  --accent-primary: #6366f1;  /* Change accent color */
  --bg-primary: #0f0f1a;      /* Change background */
}
```

### Model Settings
Configure chat settings in `main.js`:
```javascript
chatClient.settings.temperature = 0.7;
chatClient.settings.maxTokens = 2048;
```

## Troubleshooting

**Models not loading?**
- Ensure Foundry Local core is running
- Check that models are compatible (chat models only)

**Slow performance?**
- Try a smaller quantized model variant
- Ensure no other processes are using the GPU

## License

MIT

