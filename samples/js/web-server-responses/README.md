# Foundry Local Responses web service sample

This sample starts the Foundry Local OpenAI-compatible web service, then uses the official OpenAI JavaScript SDK to call the Responses API.

The important pattern is:

1. `FoundryLocalManager` handles Foundry Local setup, model download/load, web service startup, and cleanup.
1. `openai` handles the actual `/v1/responses` calls.

## Prerequisites

- Node.js 18 or later
- Internet access on first run to install npm packages, download execution providers, and download the sample model

## What gets installed

Running `npm install` in this folder installs:

| Package | Why it is used |
|---------|----------------|
| `foundry-local-sdk` | Starts Foundry Local, downloads/loads the model, and runs the local OpenAI-compatible web service. |
| `openai` | Sends Responses API requests to the local web service at `http://localhost:5764/v1`. |
| `foundry-local-sdk-winml` | Optional Windows acceleration package. npm installs it when supported and ignores it otherwise. |

The Foundry Local SDK install also provisions the native runtime files it needs, including Foundry Local Core, ONNX Runtime, and ONNX Runtime GenAI.

When you run the sample, it also downloads and loads the `qwen2.5-0.5b` model if it is not already cached.

## Run the sample

From the repository root:

```powershell
cd samples\js\web-server-responses
npm install
npm start
```

## What the sample does

The sample:

1. Initializes `FoundryLocalManager`.
1. Downloads and registers execution providers.
1. Downloads and loads `qwen2.5-0.5b`.
1. Starts the local web service at `http://localhost:5764`.
1. Uses the OpenAI JavaScript SDK with `baseURL: "http://localhost:5764/v1"`.
1. Runs a non-streaming Responses call.
1. Runs a streaming Responses call.
1. Runs a Responses function-calling flow with a sample `get_weather` tool.
1. Stops the web service and unloads the model.

## Expected output

You should see setup logs, then output similar to:

```text
Testing a non-streaming Responses call...
[ASSISTANT]: ...

Testing a streaming Responses call...
[ASSISTANT STREAM]: ...

Testing Responses tool calling...
[TOOL CALL]: get_weather(...)
[ASSISTANT FINAL]: ...
```

The exact model text can vary.

## Troubleshooting

If the sample fails while creating `FoundryLocalManager` with a native symbol error such as `Failed to resolve 'execute_command_with_binary' symbol`, the installed Foundry Local Core runtime is older than the JavaScript native addon expects. Reinstall the sample dependencies so npm can fetch the latest SDK/runtime packages:

```powershell
Remove-Item -Recurse -Force node_modules, package-lock.json
npm install
```

If port `5764` is already in use, stop the other process or update `endpointUrl` in `app.js` to an available local URL.
