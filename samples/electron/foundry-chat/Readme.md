# Foundry Local Chat Demo

A simple Electron Chat application that can chat with cloud and local models.

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
 


1. Clone the repository:
   ```powershell
   git clone https://github.com/chendrixson/FoundryChat.git
   cd FoundryChat
   ```

2. Install dependencies:
   ```powershell
   npm install
   ```

3. Set your Azure Cloud Services API key to use cloud-hosted models in [private.js](./private.js)

4. Start the application:
   ```powershell
   npm start
   ```

## Doing The Demo
[Main.js](./main.js) contains the code to hit both the azure endpoint and the local model endpoint. Comment out  the code for hitting the Azure endpoint and uncomment the code for using the foudry local endpoint. 

Change this parameter to use whicever local model you want to demo: 

const modelName = "Phi-4-mini-instruct-onnx" 

## Building the Application (not necessary for demo)

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
- `private.js` - Private configuration file

## Dependencies

- Electron - Cross-platform desktop application framework
- foundry-local-sdk - Local model integration
- OpenAI - Cloud model integration

