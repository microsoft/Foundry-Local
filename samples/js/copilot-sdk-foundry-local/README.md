# Copilot SDK + Foundry Local Sample

This sample demonstrates using [GitHub Copilot SDK](https://github.com/github/copilot-sdk) with [Foundry Local](https://github.com/microsoft/Foundry-Local) for on-device agentic AI workflows.

## What This Shows

- Bootstrapping Foundry Local with the Foundry Local SDK (service lifecycle + model management)
- Configuring Copilot SDK's **BYOK (Bring Your Own Key)** to use Foundry Local as the inference backend
- Creating a Copilot session with a **custom tool** (agentic capability)
- Streaming responses and multi-turn conversation via the Copilot SDK session API

## Prerequisites

1. **[Foundry Local](https://github.com/microsoft/Foundry-Local#installing)** installed
2. **[GitHub Copilot CLI](https://docs.github.com/en/copilot/how-tos/set-up/install-copilot-cli)** installed and authenticated
3. **Node.js 18+**

Verify prerequisites:

```bash
foundry --version
copilot --version
node --version
```

## Setup and Run

```bash
cd samples/js/copilot-sdk-foundry-local
npm install
npm start
```

## What Happens

1. **Foundry Local bootstrap** — Starts the local inference service (if not running) and downloads/loads the `gpt-oss-20b` model
2. **Copilot SDK client creation** — Creates a `CopilotClient` which communicates with the Copilot CLI over JSON-RPC
3. **BYOK session** — Creates a session with `provider: { type: "openai", baseUrl: "<foundry-local-endpoint>" }`, routing all inference through Foundry Local instead of GitHub Copilot's cloud
4. **Tool calling** — Defines a `get_system_info` tool that the model can invoke, demonstrating agentic capabilities
5. **Multi-turn conversation** — Sends a follow-up message in the same session

## Architecture

```
Your App (this sample)
     |
     ├─ foundry-local-sdk ──→ Foundry Local service (model lifecycle)
     |
     └─ @github/copilot-sdk
              |
              ├─ JSON-RPC ──→ Copilot CLI (agent orchestration)
              |
              └─ BYOK provider config
                       |
                       └─ POST /v1/chat/completions ──→ Foundry Local (inference)
                                                              |
                                                              └─ Local Model (gpt-oss-20b via ONNX Runtime)
```

## Key Configuration: BYOK Provider

The critical piece is the `provider` config in `createSession()`:

```typescript
const session = await client.createSession({
    model: modelInfo.id,
    provider: {
        type: "openai",                // Foundry Local exposes OpenAI-compatible API
        baseUrl: manager.endpoint,     // e.g., "http://localhost:5272/v1"
        apiKey: manager.apiKey,
        wireApi: "completions",        // Chat Completions API format
    },
    streaming: true,
    tools: [getSystemInfo],
});
```

This tells Copilot SDK to route inference requests to Foundry Local's endpoint instead of GitHub Copilot's cloud service. See the [Copilot SDK BYOK documentation](https://github.com/github/copilot-sdk/blob/main/docs/auth/byok.md) for all provider options.

## Related

- [Copilot SDK Integration Guide](../../../docs/copilot-sdk-integration.md) — Full integration guide with architecture details
- [Copilot SDK Getting Started](https://github.com/github/copilot-sdk/blob/main/docs/getting-started.md) — Official Copilot SDK tutorial
- [Copilot SDK BYOK Docs](https://github.com/github/copilot-sdk/blob/main/docs/auth/byok.md) — Full BYOK configuration reference
- [Foundry Local hello-foundry-local sample](../hello-foundry-local/) — Simpler sample using OpenAI client directly (no Copilot SDK)
