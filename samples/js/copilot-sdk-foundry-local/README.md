# Copilot SDK + Foundry Local Sample

This sample demonstrates using [GitHub Copilot SDK](https://github.com/github/copilot-sdk) with [Foundry Local](https://github.com/microsoft/Foundry-Local) for on-device agentic AI workflows — all inference runs locally on your machine.

> [!WARNING] 
> **GPU Required.** The Copilot SDK's agent orchestration injects a large system prompt (tool schemas, security guardrails, environment context) into every request. Combined with multi-turn conversation history, this means the local model must process a substantial input context on every turn. **A GPU with sufficient VRAM is strongly recommended**; CPU-only inference will be extremely slow (minutes per turn).

## What This Shows

- Bootstrapping Foundry Local with the Foundry Local SDK (service lifecycle + model management)
- Configuring Copilot SDK's **BYOK (Bring Your Own Key)** to use Foundry Local as the inference backend. Note that BYOK is not just authentication — it also allows you to specify a custom API endpoint and response format, enabling seamless integration with local models.
- Using Copilot's **built-in agentic tools** (file reading) powered by a local model
- Registering **custom tools** that the model can invoke during conversation
- Streaming responses and multi-turn conversation via the Copilot SDK session API

## Prerequisites

1. **[Foundry Local](https://github.com/microsoft/Foundry-Local#installing)** installed
2. **[GitHub Copilot CLI](https://docs.github.com/en/copilot/how-tos/set-up/install-copilot-cli)** installed and authenticated
3. **Node.js 18+**
4. **A GPU** is strongly recommended for reasonable performance.

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

### `app.ts` — Self-reading app (`npm start`)

```bash
npm start
```

Demonstrates Copilot's built-in agentic tools running against a local model. The app creates a BYOK session, then asks the model to **read its own source code** using Copilot's `view` tool and explain what it does. A follow-up turn tests multi-turn conversation context.

**What it shows:**
- Foundry Local bootstrap (download → load → start web service)
- BYOK session creation pointing at the local endpoint
- Copilot's built-in `view` tool reading files on disk, powered by local inference
- Streaming responses and multi-turn conversation

### `tool-calling.ts` — Custom tools (`npm run tools`)

```bash
npm run tools
```

Registers three custom tools that the model can invoke during conversation, then runs three turns — each designed to trigger a specific tool:

| Tool | What it does |
|------|-------------|
| `calculate` | Evaluates math expressions (e.g. `Math.sqrt(144) + 8 * 3`) |
| `lookup_definition` | Looks up AI/programming terms (BYOK, ONNX, RAG, etc.) |
| `get_system_info` | Returns OS, architecture, memory, CPU count, and running model |

When a tool is called you'll see `[Tool called: ...]` in the output.

**What it shows:**
- Defining custom tools with `defineTool` and Zod schemas
- Tool invocation and result handling via the Copilot SDK agent loop
- The full round-trip: model decides to call a tool → SDK executes the handler → result flows back to the model

## Configuration

### Timeout

Both examples default to **120 seconds** per model turn. Override via the `FOUNDRY_TIMEOUT_MS` environment variable:

```bash
# 3-minute timeout
FOUNDRY_TIMEOUT_MS=180000 npm start

# 5-minute timeout for tool-calling (tool round-trips take longer)
FOUNDRY_TIMEOUT_MS=300000 npm run tools
```

### Performance Notes

The Copilot CLI is a full agentic system — it injects a system prompt containing tool schemas, security guardrails, and environment context into every request sent to the model. This system prompt alone can be **40–50 KB** (~12,000+ tokens). On a GPU this is processed quickly, but on CPU-only hardware the time-to-first-token can be very long.

To mitigate this:
- **Use a GPU.** This is the single biggest improvement.
- The samples use `availableTools` to restrict which built-in tools are sent to the model, reducing the system prompt size.
- System messages include "Keep responses concise" to limit output token generation.

## How It Works

1. **Foundry Local bootstrap** — `FoundryLocalManager.create()` initializes the SDK, `model.download()` and `model.load()` prepare the model, and `manager.startWebService()` starts an OpenAI-compatible HTTP server
2. **Copilot SDK client** — `CopilotClient` communicates with the Copilot CLI over JSON-RPC
3. **BYOK session** — `createSession()` with `provider: { type: "openai", baseUrl: "<foundry-endpoint>/v1" }` routes all inference through Foundry Local instead of GitHub Copilot's cloud
4. **Tool calling** — Built-in tools (like `view`) and custom tools (like `calculate`) are available to the model; the SDK handles the tool invocation loop
5. **Multi-turn conversation** — Multiple messages in the same session share conversational context
6. **Cleanup** — `finally` block unloads the model and stops the web service

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
                       └─ POST /v1/chat/completions ──→ Foundry Local web service
                                                              |
                                                              └─ Local Model (phi-4-mini via ONNX Runtime)
```

## Key Configuration: BYOK Provider

The critical piece is the `provider` config in `createSession()`:

```typescript
const manager = FoundryLocalManager.create({
    appName: "foundry_local_samples",
    webServiceUrls: "http://localhost:6543",
});
const model = await manager.catalog.getModel("phi-4-mini");
await model.download();
await model.load();
manager.startWebService();

const session = await client.createSession({
    model: model.id,
    provider: {
        type: "openai",                // Foundry Local exposes OpenAI-compatible API
        baseUrl: "http://localhost:6543/v1",
        apiKey: "local",               // Placeholder; Foundry Local does not require auth
        wireApi: "completions",        // Chat Completions API format
    },
    streaming: true,
});
```

This tells Copilot SDK to route inference requests to Foundry Local's endpoint instead of GitHub Copilot's cloud service. See the [Copilot SDK BYOK documentation](https://github.com/github/copilot-sdk/blob/main/docs/auth/byok.md) for all provider options.

