# Copilot SDK + Foundry Local Sample

This sample demonstrates using [GitHub Copilot SDK](https://github.com/github/copilot-sdk) with [Foundry Local](https://github.com/microsoft/Foundry-Local) for on-device agentic AI workflows.

## What This Shows

- Bootstrapping Foundry Local programmatically using the Foundry Local SDK
- Configuring the provider connection for an OpenAI-compatible local endpoint
- Sending a chat completion request to a locally-running model

## Prerequisites

1. [Foundry Local](https://github.com/microsoft/Foundry-Local#installing) installed
2. Node.js 18+

## Setup and Run

```bash
cd samples/js/copilot-sdk-foundry-local
npm install
npm start
```

The sample will:
1. Start the Foundry Local service (if not already running)
2. Download and load the `phi-3.5-mini` model (if not cached)
3. Send a chat completion request to the local model via the OpenAI-compatible API
4. Print the streaming response

## How It Works

Foundry Local provides the on-device inference server with an OpenAI-compatible API. The sample uses the Foundry Local SDK for service/model lifecycle and the OpenAI client for inference — the same pattern Copilot SDK uses internally via BYOK:

```
App → Foundry Local SDK (bootstrap) → OpenAI client → Foundry Local server → Local model
```

For full agentic capabilities (tool calling, planning, multi-turn), see the [Copilot SDK integration guide](../../../docs/copilot-sdk-integration.md).
