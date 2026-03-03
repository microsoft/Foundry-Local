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
```

### Basic example (streaming + multi-turn)

```bash
npm start
```

### Tool calling example (calculator, glossary lookup, system info)

```bash
npm run tools
```

## Examples

### `app.ts` — Basic (npm start)

Bootstraps Foundry Local, creates a BYOK session, and runs a two-turn streaming conversation.

### `tool-calling.ts` — Tool Calling (npm run tools)

Registers three tools the model can invoke during conversation:

| Tool | What it does |
|------|-------------|
| `calculate` | Evaluates math expressions (e.g. `Math.sqrt(144) + 8 * 3`) |
| `lookup_definition` | Looks up AI/programming terms (BYOK, ONNX, RAG, etc.) |
| `get_system_info` | Returns OS, architecture, memory, CPU count, and running model |

Runs three turns, each designed to trigger a specific tool. When a tool is called you'll see `[Tool called: ...]` in the output.

## Configuration

### Timeout

Both examples default to **120 seconds** per model turn. On slower hardware (CPU-only, low RAM) you may need more time. Override via the `FOUNDRY_TIMEOUT_MS` environment variable:

```bash
# 3-minute timeout
FOUNDRY_TIMEOUT_MS=180000 npm start

# 5-minute timeout for tool-calling (tool round-trips take longer)
FOUNDRY_TIMEOUT_MS=300000 npm run tools
```

The Copilot SDK's built-in `sendAndWait()` also accepts an optional `timeout` parameter (default 60 000 ms). The samples use a custom `sendMessage()` helper that wraps `session.send()` with its own timeout to work around a Foundry Local streaming quirk (missing `finish_reason`). The `FOUNDRY_TIMEOUT_MS` env var controls that helper's timeout.

## What Happens

1. **Foundry Local bootstrap** — Initializes the SDK via `FoundryLocalManager.create()`, downloads and loads the `phi-4-mini` model, and starts the web service
2. **Copilot SDK client creation** — Creates a `CopilotClient` which communicates with the Copilot CLI over JSON-RPC
3. **BYOK session** — Creates a session with `provider: { type: "openai", baseUrl: "<foundry-local-endpoint>" }`, routing all inference through Foundry Local instead of GitHub Copilot's cloud
4. **Tool calling** — Tools are registered at session creation; the model can invoke them and receive results mid-conversation
5. **Multi-turn conversation** — Multiple messages in the same session share conversational context
6. **Cleanup** — Unloads the model and stops the web service on exit

## Architecture

```
Your App (this sample)
     |
     ├─ foundry-local-sdk ──→ Foundry Local (model lifecycle + web service)
     |
     └─ @github/copilot-sdk
              |
              ├─ JSON-RPC ──→ Copilot CLI (agent orchestration)
              |
              └─ BYOK provider config
                       |
                       └─ POST /v1/chat/completions ──→ Foundry Local web service (inference)
                                                              |
                                                              └─ Local Model (phi-4-mini via ONNX Runtime)
```

## Key Configuration: BYOK Provider

The critical piece is the `provider` config in `createSession()`:

```typescript
// Initialize the SDK and start the web service
const endpointUrl = "http://localhost:5764";
const manager = FoundryLocalManager.create({
    appName: "CopilotSDKFoundryLocal",
    webServiceUrls: endpointUrl,
});
const model = await manager.catalog.getModel("phi-4-mini");
await model.download();
await model.load();
manager.startWebService();
const endpoint = manager.urls[0]; // Matches the configured webServiceUrls

// Create a BYOK session pointing at the Foundry Local web service
const session = await client.createSession({
    model: model.id,
    provider: {
        type: "openai",                // Foundry Local exposes OpenAI-compatible API
        baseUrl: endpoint,             // From manager.urls after startWebService()
        apiKey: "local",               // Placeholder; Foundry Local does not require auth
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
