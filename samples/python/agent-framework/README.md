# Agent Framework + Foundry Local

A multi-agent orchestration sample powered by [Microsoft Agent Framework](https://pypi.org/project/agent-framework-core/) and [Foundry Local](https://foundrylocal.ai). All inference runs **on-device** through Foundry Local's OpenAI-compatible endpoint — no cloud API keys required.

## What It Does

Five specialised agents collaborate in configurable pipelines to research a user question:

| Agent       | Role                                            |
|-------------|-------------------------------------------------|
| **Planner** | Breaks the question into 2-4 sub-tasks          |
| **Retriever** | Searches local documents for relevant excerpts |
| **Critic**  | Reviews output for gaps and contradictions       |
| **Writer**  | Synthesises a final report with citations        |
| **ToolAgent** | Runs deterministic tools (word count, keywords)|

### Orchestration Patterns

- **Sequential** — Planner → Retriever → Critic ⇄ Retriever → Writer
- **Concurrent** — Retriever ‖ ToolAgent (fan-out with `asyncio.gather`)
- **Full (hybrid)** — Sequential planning, concurrent retrieval, feedback loop, then synthesis

### Interactive Demos

The web UI also ships five standalone demos: Weather Tools, Math Agent, Sentiment Analyser, Code Reviewer, and Multi-Agent Debate.

## Prerequisites

- **Python 3.10+**
- **Foundry Local** installed and available on PATH — see [foundrylocal.ai](https://foundrylocal.ai)

## Quick Start

```bash
# Clone the repo and navigate to this sample
cd samples/python/agent-framework

# Create a virtual environment (recommended)
python -m venv .venv
source .venv/bin/activate   # Linux/macOS
.venv\Scripts\activate      # Windows

# Install dependencies
pip install -e ".[dev]"

# (Optional) copy and edit the environment config
cp .env.example .env

# Run the web UI (starts Flask on http://localhost:5000)
python -m src.app --web

# Or run a question directly from the CLI
python -m src.app "What orchestration patterns exist for multi-agent systems?"

# Choose orchestration mode (sequential or full)
python -m src.app --mode sequential "Explain Foundry Local architecture"
```

The web UI starts at **http://localhost:5000**. On first run, Foundry Local will download the model if it is not already cached.

## Project Structure

```
agent-framework/
├── data/                       # Sample documents loaded by the Retriever agent
│   ├── agent_framework_guide.md
│   ├── foundry_local_overview.md
│   └── orchestration_patterns.md
├── src/app/
│   ├── __init__.py
│   ├── __main__.py             # CLI entrypoint (web / cli / tools)
│   ├── foundry_boot.py         # Bootstrap Foundry Local, get connection info
│   ├── agents.py               # Agent factories and tool functions
│   ├── documents.py            # Document loader with chunking
│   ├── orchestrator.py         # Sequential, concurrent, and hybrid pipelines
│   ├── tool_demo.py            # Standalone tool-calling demo
│   ├── web.py                  # Flask server with SSE streaming
│   ├── templates/
│   │   └── index.html          # Web UI with real-time pipeline visualisation
│   └── demos/                  # Interactive demo modules
│       ├── __init__.py
│       ├── registry.py         # Demo registry
│       ├── weather_tools.py
│       ├── math_agent.py
│       ├── sentiment_analyzer.py
│       ├── code_reviewer.py
│       └── multi_agent_debate.py
├── tests/
│   └── test_smoke.py           # Smoke tests (imports, doc loader, etc.)
├── pyproject.toml              # Project metadata & dependencies
├── requirements.txt            # Pip-installable dependencies
├── .env.example                # Environment variable template
└── README.md                   # This file
```

## Configuration

| Variable           | Default        | Description                                 |
|--------------------|----------------|---------------------------------------------|
| `MODEL_ALIAS`      | `qwen2.5-0.5b` | Foundry Local model alias                   |
| `DOCS_PATH`        | `./data`       | Path to documents folder                     |
| `LOG_LEVEL`        | `INFO`         | Python logging level                         |
| `FOUNDRY_ENDPOINT` | *(auto)*       | Override to skip local bootstrap              |
| `FOUNDRY_API_KEY`  | `none`         | API key when using an external endpoint       |

## How It Works

1. **Bootstrap** — `FoundryLocalBootstrapper` starts the Foundry Local service, resolves the model alias, and downloads the model if not cached.
2. **Document loading** — Markdown and text files from `data/` are chunked and passed as context to the Retriever agent.
3. **Orchestration** — agents are wired together per the selected pattern. Each step emits structured JSON events.
4. **Streaming** — the Flask server streams events via SSE so the web UI can render pipeline progress in real time.

## Tests

```bash
pytest tests/ -v
```

The smoke tests verify imports, document loading, the bootstrapper's environment override path, and the demo registry.

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| `ModuleNotFoundError: agent_framework` | `pip install agent-framework-core==1.0.0b260130` |
| Model download hangs | Check network and ensure Foundry Local is on PATH |
| `Connection refused` on port 5273 | Foundry Local service failed to start — run `foundry-local` manually to see errors |
| Flask port 5000 in use | Set `FLASK_PORT` env var or kill the conflicting process |

## License

This sample is provided under the [MIT License](../../../LICENSE).
