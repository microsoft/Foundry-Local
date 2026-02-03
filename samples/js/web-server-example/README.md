# Chat completions using an OpenAI-compatible web server

This sample demonstrates how to use the Foundry Local SDK to perform chat completions using an OpenAI-compatible web server. It initializes the SDK with the server URL, selects a model, and sends a chat completion request with a system prompt and user message.

## Prerequisites
- Ensure you have Node.js installed (version 20 or higher is recommended).

## Setup project

Navigate to the sample directory, setup the project, and install the required packages.

1. Navigate to the sample directory and setup the project:
    ```bash
    cd samples/js/web-server-example
    npm init -y
    npm pkg set type=module
    ```
1. Install the Foundry Local and OpenAI packages:
    ```bash
    npm install foundry-local-sdk
    npm install openai
    ```

> [!NOTE]
> On Windows, NPU models are not currently available for the JavaScript SDK. These will be enabled in a subsequent release.

## Run the sample

Run the sample script using Node.js:

```bash
cd samples/js/web-server-example
node app.js
```