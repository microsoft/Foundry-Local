# Using GitHub Copilot SDK with Foundry Local

## Overview

For **agentic workflows** — tool calling, multi-step planning, and multi-turn conversations — you can use [GitHub Copilot SDK](https://github.com/github/copilot-sdk) with Foundry Local as the on-device inference backend. Copilot SDK provides the agentic orchestration layer while Foundry Local handles local model execution.

This approach requires **no changes** to Foundry Local or its APIs. Copilot SDK connects to Foundry Local's OpenAI-compatible endpoint via its [Bring Your Own Key (BYOK)](https://github.com/github/copilot-sdk/blob/main/docs/auth/byok.md) feature.

## Architecture

```
Your Application
     ↓
GitHub Copilot SDK (agentic orchestration: tools, planning, multi-turn)
     ↓ BYOK config: type "openai"
Foundry Local (on-device inference server)
     ↓ POST /v1/chat/completions (OpenAI-compatible)
Local Model (e.g., phi-3.5-mini via ONNX Runtime)
```

## Quick Start

### Prerequisites

1. **Install Foundry Local**
   - Windows: `winget install Microsoft.FoundryLocal`
   - macOS: `brew install microsoft/foundrylocal/foundrylocal`

2. **Download a model**
   ```bash
   foundry model run phi-3.5-mini
   ```

3. **Install GitHub Copilot SDK** — see [Copilot SDK docs](https://github.com/github/copilot-sdk)

### Node.js / TypeScript

```typescript
import { FoundryLocalManager } from "foundry-local-sdk";

// Step 1: Bootstrap Foundry Local (starts service + loads model)
const manager = new FoundryLocalManager();
const modelInfo = await manager.init("phi-3.5-mini");
console.log("Endpoint:", manager.endpoint);
console.log("Model:", modelInfo.id);

// Step 2: Configure Copilot SDK to use Foundry Local as inference backend
// Use BYOK with type "openai" — Foundry Local's API is OpenAI-compatible
const providerConfig = {
    type: "openai",
    baseUrl: manager.endpoint,   // e.g., "http://localhost:5272/v1"
    apiKey: manager.apiKey,
    model: modelInfo.id,
};

// Step 3: Use providerConfig when creating a Copilot SDK session
// See Copilot SDK docs for full session API usage
```

### Python

```python
from foundry_local import FoundryLocalManager

# Step 1: Bootstrap Foundry Local (starts service + loads model)
manager = FoundryLocalManager("phi-3.5-mini")
model_info = manager.get_model_info("phi-3.5-mini")
print(f"Endpoint: {manager.endpoint}")
print(f"Model: {model_info.id}")

# Step 2: Configure Copilot SDK to use Foundry Local
provider_config = {
    "type": "openai",
    "base_url": manager.endpoint,   # e.g., "http://localhost:5272/v1"
    "api_key": manager.api_key,
    "model": model_info.id,
}

# Step 3: Use provider_config when creating a Copilot SDK session
# See Copilot SDK docs for full session API usage
```

## When to Use Which Approach

| Scenario | Recommended Approach |
|----------|---------------------|
| Simple chat completions | Foundry Local SDK + OpenAI client ([existing samples](../samples/)) |
| **Agentic workflows** (tools, planning, multi-turn) | **Copilot SDK + Foundry Local** (this guide) |
| Model management only (download, load, unload) | Foundry Local SDK directly |
| Production cloud inference with agentic features | Copilot SDK with cloud providers |

> **Note:** The existing Foundry Local SDKs (Python, JavaScript, C#, Rust) remain fully supported. This guide provides an additional option for developers who need agentic orchestration capabilities.

## Limitations

- **Tool calling**: Depends on model support. Not all Foundry Local models support function calling. Check model capabilities with `foundry model ls`.
- **Preview APIs**: Both Foundry Local's REST API and Copilot SDK may have breaking changes during preview.
- **Model size**: On-device models are smaller than cloud models. Agentic performance (multi-step planning, complex tool use) may vary compared to cloud-hosted models.
- **Platform**: Foundry Local supports Windows (x64/arm64) and macOS (Apple Silicon).

## Related Links

- [GitHub Copilot SDK](https://github.com/github/copilot-sdk) — Multi-platform SDK for agentic workflows
- [Copilot SDK BYOK Documentation](https://github.com/github/copilot-sdk/blob/main/docs/auth/byok.md) — Full BYOK configuration reference
- [Foundry Local Samples](../samples/) — Existing samples using Foundry Local SDK + OpenAI client
- [Foundry Local Documentation (Microsoft Learn)](https://learn.microsoft.com/azure/ai-foundry/foundry-local/)
