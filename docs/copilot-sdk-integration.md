# Using GitHub Copilot SDK with Foundry Local

## Overview

For **agentic workflows** — tool calling, multi-step planning, and multi-turn conversations — you can use [GitHub Copilot SDK](https://github.com/github/copilot-sdk) with Foundry Local as the on-device inference backend. Copilot SDK provides the agentic orchestration layer while Foundry Local handles local model execution.

This approach requires **no changes** to Foundry Local or its APIs. Copilot SDK connects to Foundry Local's OpenAI-compatible endpoint via its [Bring Your Own Key (BYOK)](https://github.com/github/copilot-sdk/blob/main/docs/auth/byok.md) feature.

## Architecture

```
Your Application
     |
     ├─ foundry-local-sdk ──→ Foundry Local service (model lifecycle)
     |
     └─ @github/copilot-sdk (CopilotClient)
              |
              ├─ JSON-RPC ──→ Copilot CLI (agent orchestration, tool execution)
              |
              └─ BYOK provider: { type: "openai", baseUrl: "http://localhost:5272/v1" }
                       |
                       └─ POST /v1/chat/completions ──→ Foundry Local (on-device inference)
                                                              |
                                                              └─ Local Model (e.g., gpt-oss-20b via ONNX Runtime)
```

**Key components:**

- **Foundry Local SDK** (`foundry-local-sdk`) — Manages the local inference service lifecycle (start, model download/load)
- **Copilot SDK** (`@github/copilot-sdk`) — Provides `CopilotClient` for agentic orchestration (sessions, tools, streaming, multi-turn)
- **Copilot CLI** — Background process that the SDK communicates with over JSON-RPC. Handles agent orchestration and tool execution
- **BYOK** — Routes inference requests from Copilot SDK to Foundry Local's OpenAI-compatible endpoint instead of GitHub Copilot's cloud

## Prerequisites

1. **Install Foundry Local**
   - Windows: `winget install Microsoft.FoundryLocal`
   - macOS: `brew install microsoft/foundrylocal/foundrylocal`

2. **Install GitHub Copilot CLI** and authenticate
   - See [Copilot CLI installation guide](https://docs.github.com/en/copilot/how-tos/set-up/install-copilot-cli)
   - Verify: `copilot --version`

3. **Download a model**
   ```bash
   foundry model run gpt-oss-20b
   ```

## Quick Start: Node.js / TypeScript

```typescript
import { CopilotClient, defineTool } from "@github/copilot-sdk";
import { FoundryLocalManager } from "foundry-local-sdk";

// Bootstrap Foundry Local (starts service + loads model)
const manager = new FoundryLocalManager();
const modelInfo = await manager.init("gpt-oss-20b");

// Create a Copilot SDK client (communicates with Copilot CLI over JSON-RPC)
const client = new CopilotClient();

// Create a session with BYOK pointing to Foundry Local
const session = await client.createSession({
    model: modelInfo.id,
    provider: {
        type: "openai",
        baseUrl: manager.endpoint,     // e.g., "http://localhost:5272/v1"
        apiKey: manager.apiKey,
        wireApi: "completions",        // Foundry Local uses Chat Completions API
    },
    streaming: true,
});

// Subscribe to streaming response chunks
session.on("assistant.message_delta", (event) => {
    process.stdout.write(event.data.deltaContent);
});

// Send a message and wait for the complete response
await session.sendAndWait({ prompt: "What is the golden ratio?" });

// Clean up
await session.destroy();
await client.stop();
```

Install and run:

```bash
npm install @github/copilot-sdk foundry-local-sdk tsx
npx tsx app.ts
```

## Quick Start: Python

```python
import asyncio
import sys
from copilot import CopilotClient
from copilot.generated.session_events import SessionEventType
from foundry_local import FoundryLocalManager

async def main():
    # Bootstrap Foundry Local
    manager = FoundryLocalManager("gpt-oss-20b")
    model_info = manager.get_model_info("gpt-oss-20b")

    # Create a Copilot SDK client
    client = CopilotClient()
    await client.start()

    # Create a session with BYOK pointing to Foundry Local
    session = await client.create_session({
        "model": model_info.id,
        "provider": {
            "type": "openai",
            "base_url": manager.endpoint,
            "api_key": manager.api_key,
            "wire_api": "completions",
        },
        "streaming": True,
    })

    # Subscribe to streaming response chunks
    def on_event(event):
        if event.type == SessionEventType.ASSISTANT_MESSAGE_DELTA:
            sys.stdout.write(event.data.delta_content)
            sys.stdout.flush()

    session.on(on_event)

    await session.send_and_wait({"prompt": "What is the golden ratio?"})

    await session.destroy()
    await client.stop()

asyncio.run(main())
```

Install and run:

```bash
pip install github-copilot-sdk foundry-local-sdk
python app.py
```

## Adding Custom Tools

Copilot SDK supports custom tools that the model can invoke during a conversation. This enables agentic workflows where the model can call your code:

### Node.js / TypeScript

```typescript
import { CopilotClient, defineTool } from "@github/copilot-sdk";
import { FoundryLocalManager } from "foundry-local-sdk";

const manager = new FoundryLocalManager();
const modelInfo = await manager.init("gpt-oss-20b");

// Define a tool the model can call
const getSystemInfo = defineTool("get_system_info", {
    description: "Get information about the local AI system",
    parameters: {
        type: "object",
        properties: {
            query: { type: "string", description: "What to look up: 'model' or 'endpoint'" },
        },
        required: ["query"],
    },
    handler: async (args: { query: string }) => {
        if (args.query === "model") {
            return { modelId: modelInfo.id, runtime: "ONNX Runtime" };
        }
        return { url: manager.endpoint, protocol: "OpenAI-compatible" };
    },
});

const client = new CopilotClient();
const session = await client.createSession({
    model: modelInfo.id,
    provider: {
        type: "openai",
        baseUrl: manager.endpoint,
        apiKey: manager.apiKey,
        wireApi: "completions",
    },
    streaming: true,
    tools: [getSystemInfo],
});

session.on("assistant.message_delta", (event) => {
    process.stdout.write(event.data.deltaContent);
});

await session.sendAndWait({
    prompt: "What model am I running locally? Use the get_system_info tool to find out.",
});

await session.destroy();
await client.stop();
```

### Python

```python
from copilot import CopilotClient
from copilot.tools import define_tool
from pydantic import BaseModel, Field

class SystemInfoParams(BaseModel):
    query: str = Field(description="What to look up: 'model' or 'endpoint'")

@define_tool(description="Get information about the local AI system")
async def get_system_info(params: SystemInfoParams) -> dict:
    if params.query == "model":
        return {"modelId": model_info.id, "runtime": "ONNX Runtime"}
    return {"url": manager.endpoint, "protocol": "OpenAI-compatible"}

# Pass tools when creating the session:
session = await client.create_session({
    "model": model_info.id,
    "provider": { ... },  # BYOK config as above
    "tools": [get_system_info],
})
```

## BYOK Provider Configuration Reference

The `provider` object in `createSession()` configures where Copilot SDK sends inference requests:

| Field | Type | Description |
|-------|------|-------------|
| `type` | `"openai"` | Provider type. Use `"openai"` for Foundry Local (OpenAI-compatible) |
| `baseUrl` | string | Foundry Local endpoint, e.g., `"http://localhost:5272/v1"` |
| `apiKey` | string | API key (optional for local endpoints) |
| `wireApi` | `"completions"` \| `"responses"` | API format. Use `"completions"` for Foundry Local |

For the full BYOK reference including Azure, Anthropic, and other providers, see [Copilot SDK BYOK docs](https://github.com/github/copilot-sdk/blob/main/docs/auth/byok.md).

## When to Use Which Approach

| Scenario | Recommended Approach |
|----------|---------------------|
| Simple chat completions | Foundry Local SDK + OpenAI client ([existing samples](../samples/)) |
| **Agentic workflows** (tools, planning, multi-turn) | **Copilot SDK + Foundry Local** (this guide) |
| Model management only (download, load, unload) | Foundry Local SDK directly |
| Production cloud inference with agentic features | Copilot SDK with cloud providers |

> **Note:** The existing Foundry Local SDKs (Python, JavaScript, C#, Rust) remain fully supported. This guide provides an additional option for developers who need agentic orchestration capabilities.

## Limitations

- **Copilot CLI required**: The Copilot SDK requires the [Copilot CLI](https://docs.github.com/en/copilot/how-tos/set-up/install-copilot-cli) to be installed and authenticated. The SDK communicates with it over JSON-RPC.
- **Tool calling**: Depends on model support. Not all Foundry Local models support function calling. Check model capabilities with `foundry model ls`.
- **Preview APIs**: Both Foundry Local's REST API and Copilot SDK may have breaking changes during preview.
- **Model size**: On-device models are smaller than cloud models. Agentic performance (multi-step planning, complex tool use) may vary compared to cloud-hosted models.
- **Platform**: Foundry Local supports Windows (x64/arm64) and macOS (Apple Silicon).

## Working Sample

See the complete working sample at [`samples/js/copilot-sdk-foundry-local/`](../samples/js/copilot-sdk-foundry-local/) which demonstrates bootstrapping, BYOK configuration, tool calling, streaming, and multi-turn conversation.

## Related Links

- [GitHub Copilot SDK](https://github.com/github/copilot-sdk) — Multi-platform SDK for agentic workflows
- [Copilot SDK Getting Started](https://github.com/github/copilot-sdk/blob/main/docs/getting-started.md) — Official tutorial
- [Copilot SDK BYOK Documentation](https://github.com/github/copilot-sdk/blob/main/docs/auth/byok.md) — Full BYOK configuration reference
- [Foundry Local Samples](../samples/) — Existing samples using Foundry Local SDK + OpenAI client
- [Foundry Local Documentation (Microsoft Learn)](https://learn.microsoft.com/azure/ai-foundry/foundry-local/)
