# Foundry Local Chat Demo

A simple Electron Chat application that can chat with cloud and Foundry local models.

## Prerequisites

- Node.js (v16 or higher)
  - To install Node.js on Windows, run:
    ```powershell
    winget install OpenJS.NodeJS
    ```
  - npm comes bundled with Node.js

## Setup Instructions
1. Download the .MSIX and install Foundry for your processor:
   [Foundry LKG Release](https://github.com/ai-platform-microsoft/WindowsAIFoundry/releases/tag/0.2.9261.3945)
   Then install it using the following powershell command.
   ```powershell
   add-appxpackage <foundryfile>.msix
   ```
 
2. Install dependencies:
   ```powershell
   npm install
   ```

3. Set the following environment variables to your Cloud AI Service
   ```powershell
   YOUR_API_KEY
   YOUR_ENDPOINT
   YOUR_MODEL_NAME
   ```

4. Start the application:
   ```powershell
   npm start
   ```

## Building the Application (not necessary for testing)

To build the application for your platform:
```powershell
# For all platforms
npm run build

# For Windows specifically
npm run build:win
```

The built application will be available in the `dist` directory.

## Project Structure

- `main.js` - Main Electron process file
- `chat.html` - Main application window
- `preload.cjs` - Preload script for secure IPC communication

## Dependencies

- Electron - Cross-platform desktop application framework
- foundry-local-sdk - Local model integration
- OpenAI - Cloud model integration

