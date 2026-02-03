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
    ```bash
    npm install foundry-local-sdk
    ```

> [!NOTE]
> On Windows, NPU models are not currently available for the JavaScript SDK. These will be enabled in a subsequent release.

## Run the sample

Run the sample script using Node.js:

```bash
cd samples/js/native-chat-completions
node app.js
```