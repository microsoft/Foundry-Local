# Local CAG – Context-Augmented Generation with Foundry Local

A fully offline **Context-Augmented Generation (CAG)** sample application that runs an AI support agent entirely on-device using [Foundry Local](https://foundrylocal.ai).

## What is CAG?

CAG (Context-Augmented Generation) pre-loads **all** domain documents at startup and injects them into the AI prompt — no vector database, no embeddings, no retrieval step. This makes it ideal for:

- **Offline / air-gapped** environments (e.g., field operations)
- **Small-to-medium knowledge bases** (dozens of documents)
- **Low-latency responses** — no retrieval round-trip
- **Simple deployment** — no external dependencies beyond Foundry Local

## Architecture

```
┌─────────────┐    ┌──────────────────┐    ┌────────────────────┐
│  Browser UI  │───▶│  Express Server   │───▶│  Foundry Local SDK │
│  (index.html)│◀───│  (server.js)      │◀───│  (in-process)      │
└─────────────┘    └──────────────────┘    └────────────────────┘
                          │                          │
                   ┌──────┴───────┐           ┌──────┴───────┐
                   │  ChatEngine   │           │  Model (SLM)  │
                   │  + Context    │           │  qwen / phi   │
                   └──────────────┘           └──────────────┘
```

1. **Startup**: All markdown documents in `docs/` are loaded into memory.
2. **Model selection**: The SDK auto-selects the best model for the device's RAM.
3. **Query**: Each user question is matched to the most relevant docs via keyword scoring, then injected into the prompt alongside the question.
4. **Inference**: Foundry Local runs the model in-process — no HTTP server needed.

## Prerequisites

- **Node.js 20+**
- **Foundry Local** installed — see [foundrylocal.ai](https://foundrylocal.ai)

## Quick Start

```bash
# Install dependencies
npm install

# Start the server
npm start
```

Open [http://localhost:3000](http://localhost:3000) in your browser. The UI shows real-time progress as the model loads.

## Configuration

Set these environment variables (all optional):

| Variable | Default | Description |
|----------|---------|-------------|
| `FOUNDRY_MODEL` | *(auto-select)* | Force a specific model alias (e.g., `phi-3.5-mini`) |
| `PORT` | `3000` | Server port |
| `HOST` | `localhost` | Server bind address |

## Adding Domain Documents

Place markdown files in the `docs/` folder with YAML front-matter:

```markdown
---
title: Your Document Title
category: Safety
id: unique-doc-id
---

# Your Document Title

Content goes here...
```

The engine loads all `.md` files at startup and makes them available to the AI.

## Project Structure

```
local-cag/
├── package.json
├── README.md
├── docs/                    # Domain knowledge (markdown with front-matter)
│   ├── gas-leak-detection.md
│   ├── emergency-shutdown.md
│   ├── pressure-testing.md
│   ├── ppe-requirements.md
│   └── valve-inspection.md
├── public/
│   └── index.html           # Web UI with loading overlay + chat
└── src/
    ├── server.js             # Express server with SSE status + chat endpoints
    ├── chatEngine.js         # CAG engine: SDK init, model selection, inference
    ├── config.js             # Configuration (env vars + defaults)
    ├── context.js            # Document loading, parsing, keyword-based selection
    ├── modelSelector.js      # Dynamic model selection based on device RAM
    └── prompts.js            # System prompts (full + compact/edge mode)
```

## Key Features

- **Dynamic model selection** — automatically picks the best model for the device's available RAM
- **Cache-aware** — skips download if the model is already in the Foundry cache
- **Edge mode** — toggle compact prompts for smaller models or constrained devices
- **SSE progress** — real-time loading status streamed to the browser
- **Keyword-based doc selection** — only the most relevant documents are injected per query
- **No internet required** — fully offline after initial model download

## How It Differs From RAG

| Feature | CAG (this sample) | RAG |
|---------|-------------------|-----|
| Document loading | All at startup | On-demand retrieval |
| Vector database | Not needed | Required |
| Embeddings | Not needed | Required |
| Latency | Lower (no retrieval) | Higher (search + retrieve) |
| Knowledge base size | Small–medium | Any size |
| Complexity | Simpler | More complex |

## Learn More

- [Foundry Local Documentation](https://foundrylocal.ai)
- [Foundry Local SDK (npm)](https://www.npmjs.com/package/foundry-local-sdk)
- [RAG sample](../local-rag/) — for larger knowledge bases with vector retrieval
