# Chat completions using an OpenAI-compatible web server

This sample demonstrates how to use the Foundry Local SDK to perform chat completions using an OpenAI-compatible web server. It initializes the SDK with the server URL, selects a model, and sends a chat completion request with a system prompt and user message.

## Prerequisites
- Ensure you have Node.js installed (version 20 or higher is recommended).

## Setup project

Navigate to the sample directory and install the required packages.

### Windows

1. Navigate to the sample directory and set the project type to module:
    ```bash
    cd samples/js/web-server-example
    npm pkg set type=module
    ```
1. Install the Foundry Local and OpenAI packages:
    ```bash
    npm install --winml foundry-local-sdk
    npm install openai
    ```
    > [!NOTE]
    > The `--winml` flag installs the Windows-specific package that uses Windows Machine Learning (WinML) for hardware acceleration on compatible devices.    

### MacOS and Linux

1. Navigate to the sample directory and set the project type to module:
    ```bash
    cd samples/js/web-server-example
    npm pkg set type=module
    ```
1. Install the Foundry Local and OpenAI packages:
    ```bash
    npm install foundry-local-sdk
    npm install openai
    ```

## Run the sample

Run the sample script using Node.js:

```bash
cd samples/js/web-server-example
node app.js
```