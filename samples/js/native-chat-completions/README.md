# Native chat completions with Foundry Local SDK

This sample demonstrates how to use the Foundry Local SDK to perform native chat completions using a local model. It initializes the SDK, selects a model, and sends a chat completion request with a system prompt and user message.

## Prerequisites
- Ensure you have Node.js installed (version 20 or higher is recommended).

## Setup project

Navigate to the sample directory, setup the project, and install the Foundry Local SDK package.

1. Navigate to the sample directory and setup the project:
    ```bash
    cd samples/js/native-chat-completions
    npm init -y
    npm pkg set type=module
    ```

1. Install the Foundry Local SDK package:

    **macOS / Linux:**
    ```bash
    npm install --foreground-scripts foundry-local-sdk@0.9.0-rc2
    ```

    **Windows:**
    ```bash
    npm install --foreground-scripts --winml foundry-local-sdk@0.9.0-rc2
    ```

## Workaround for macOS / Linux

> **Note:** There is a known issue where ONNX Runtime is not picked up on macOS / Linux. This will be fixed in ORT 1.24.3. In the meantime, add the ONNX Runtime native library to your library path before running the sample:
>
> **macOS:** > ```bash
> export DYLD_LIBRARY_PATH=$DYLD_LIBRARY_PATH:$(pwd)/node_modules/@foundry-local-core/darwin-arm64
> ```
>
> **Linux:**
> ```bash
> export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/node_modules/@foundry-local-core/linux-x64
> ```
>
> Run this from the sample directory after installing dependencies. The platform-specific path (e.g., `darwin-arm64`, `linux-x64`) will vary depending on your system architecture.

## Run the sample

Run the sample script using Node.js:

```bash
cd samples/js/native-chat-completions
node app.js
```