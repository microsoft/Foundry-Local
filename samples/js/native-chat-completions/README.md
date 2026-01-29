# Native chat completions with Foundry Local SDK

This sample demonstrates how to use the Foundry Local SDK to perform native chat completions using a local model. It initializes the SDK, selects a model, and sends a chat completion request with a system prompt and user message.

## Prerequisites
- Ensure you have Node.js installed (version 20 or higher is recommended).

## Setup project

Navigate to the sample directory and install the Foundry Local SDK package.

### Windows

1. Navigate to the sample directory and set the project type to module:
    ```bash
    cd samples/js/native-chat-completions
    npm pkg set type=module
    ```
1. Install the Foundry Local SDK package:
    ```bash
    npm install --winml foundry-local-sdk
    ```
    
> [!NOTE]
> The `--winml` flag installs the Windows-specific package that uses Windows Machine Learning (WinML) for hardware acceleration on compatible devices.    

### MacOS and Linux

1. Navigate to the sample directory and set the project type to module:
    ```bash
    cd samples/js/native-chat-completions
    npm pkg set type=module
    ```
1. Install the Foundry Local SDK package:
    ```bash
    npm install foundry-local-sdk
    ```

## Run the sample

Run the sample script using Node.js:

```bash
node app.js
```