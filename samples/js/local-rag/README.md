# Local RAG – Retrieval-Augmented Generation with Foundry Local

A fully offline **Retrieval-Augmented Generation (RAG)** sample application that runs an AI support agent entirely on-device using [Foundry Local](https://foundrylocal.ai).

## What is RAG?

RAG (Retrieval-Augmented Generation) **chunks documents, indexes them with term-frequency vectors, and retrieves only the most relevant chunks** via cosine similarity at query time — no cloud APIs, no embedding models, no external vector databases. This makes it ideal for:

- **Large knowledge bases** — scales beyond what fits in a single prompt
- **Offline / air-gapped** environments (e.g., field operations)
- **Dynamic content** — upload new documents at runtime via the web UI
- **Precise answers** — retrieval focuses the model on the most relevant content

## Architecture

```
┌─────────────┐    ┌──────────────────┐    ┌────────────────────┐
│  Browser UI  │───▶│  Express Server   │───▶│  Foundry Local SDK │
│  (index.html)│◀───│  (server.js)      │◀───│  (in-process)      │
└─────────────┘    └──────────────────┘    └────────────────────┘
                          │                          │
                   ┌──────┴───────┐           ┌──────┴───────┐
                   │  ChatEngine   │           │  Model (SLM)  │
                   │  + VectorStore│           │  phi-3.5-mini │
                   └──────┬───────┘           └──────────────┘
                          │
                   ┌──────┴───────┐
                   │   SQLite DB   │
                   │  (TF vectors) │
                   └──────────────┘
```

1. **Ingest**: Documents in `docs/` are chunked (200 tokens, 25-token overlap) and stored in SQLite with term-frequency vectors and an inverted index.
2. **Query**: Each user question is vectorised using term-frequency, then cosine similarity finds the top-K most relevant chunks.
3. **Prompt**: Retrieved chunks are injected into the system prompt with source citations.
4. **Inference**: Foundry Local runs the model in-process — no external HTTP server needed.

## Prerequisites

- **Node.js 20+**
- **Foundry Local** installed — see [foundrylocal.ai](https://foundrylocal.ai)

## Quick Start

```bash
# Install dependencies
npm install

# Ingest sample documents into the vector store
npm run ingest

# Start the server
npm start
```

Open [http://localhost:3000](http://localhost:3000) in your browser. The UI shows real-time progress as the model loads.

## Configuration

Set these environment variables (all optional):

| Variable | Default | Description |
|----------|---------|-------------|
| `FOUNDRY_MODEL` | `phi-3.5-mini` | Model alias to use |
| `PORT` | `3000` | Server port |
| `HOST` | `127.0.0.1` | Server bind address |

## Adding Documents

### Option 1: File System

Place markdown files in the `docs/` folder with YAML front-matter, then re-run `npm run ingest`:

```markdown
---
title: Your Document Title
category: Safety
id: unique-doc-id
---

# Your Document Title

Content goes here...
```

### Option 2: Web UI Upload

Click the **📄** button in the chat interface to upload `.md` or `.txt` files at runtime. Documents are chunked and indexed immediately — no restart required.

## Project Structure

```
local-rag/
├── package.json
├── README.md
├── docs/                    # Domain knowledge (markdown with front-matter)
│   ├── gas-leak-detection.md
│   ├── emergency-shutdown.md
│   ├── pressure-testing.md
│   ├── ppe-requirements.md
│   └── valve-inspection.md
├── public/
│   └── index.html           # Web UI with upload, chat, source citations
├── data/                    # Created at ingest time
│   └── rag.db               # SQLite vector store
└── src/
    ├── server.js             # Express server with SSE status + chat + upload
    ├── chatEngine.js         # RAG engine: SDK init, retrieval, inference
    ├── config.js             # Configuration (model, chunking, paths)
    ├── chunker.js            # Document parsing, chunking, term-frequency math
    ├── vectorStore.js        # SQLite-backed vector store with inverted index
    ├── ingest.js             # Batch document ingestion script
    └── prompts.js            # System prompts (full + compact/edge mode)
```

## Key Features

- **Cache-aware** — skips model download if already in the Foundry cache
- **Term-frequency vector search** — no embedding model needed; lightweight and fast
- **SQLite storage** — single-file database, no external services
- **Runtime document upload** — add documents via the web UI without restarting
- **Source citations** — each response shows which chunks were used and their relevance scores
- **SSE progress** — real-time loading status streamed to the browser
- **Edge mode** — toggle compact prompts for smaller models or constrained devices
- **No internet required** — fully offline after initial model download

## How It Differs From CAG

| Feature | RAG (this sample) | CAG |
|---------|-------------------|-----|
| Document loading | Chunked + indexed | All loaded at startup |
| Vector search | Term-frequency + cosine similarity | Keyword scoring |
| Storage | SQLite database | In-memory |
| Knowledge base size | Any size | Small–medium |
| Runtime upload | Yes | No |
| Source citations | Chunk-level with scores | Document-level |
| Complexity | More complex | Simpler |

## Learn More

- [Foundry Local Documentation](https://foundrylocal.ai)
- [Foundry Local SDK (npm)](https://www.npmjs.com/package/foundry-local-sdk)
- [CAG sample](../local-cag/) — for simpler use-cases where all docs fit in one prompt
