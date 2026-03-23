"""
CLI Entry Point
────────────────
Run as:  python -m src.app
"""

from __future__ import annotations

import argparse
import asyncio
import logging
import os

from dotenv import load_dotenv
from rich.console import Console
from rich.logging import RichHandler

load_dotenv()
console = Console()


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Agent Framework + Foundry Local — Multi-Agent Research Demo",
    )
    parser.add_argument("question", nargs="?", help="Research question (CLI mode)")
    parser.add_argument("--docs", default=os.getenv("DOCS_PATH", "./data"), help="Documents folder")
    parser.add_argument("--model", default=os.getenv("MODEL_ALIAS", "qwen2.5-0.5b"), help="Model alias")
    parser.add_argument("--mode", choices=["sequential", "full"], default="full", help="Orchestration mode")
    parser.add_argument("--web", action="store_true", help="Start Flask web server")
    parser.add_argument("--port", type=int, default=5000, help="Web server port")
    parser.add_argument("--log-level", default=os.getenv("LOG_LEVEL", "INFO"), help="Logging level")
    args = parser.parse_args()

    logging.basicConfig(
        level=getattr(logging, args.log_level.upper(), logging.INFO),
        format="%(message)s",
        handlers=[RichHandler(rich_tracebacks=True)],
    )

    os.environ["DOCS_PATH"] = args.docs
    os.environ["MODEL_ALIAS"] = args.model

    from .foundry_boot import FoundryLocalBootstrapper

    boot = FoundryLocalBootstrapper(alias=args.model)
    conn = boot.bootstrap()

    if args.web or args.question is None:
        # Web mode
        from .web import create_app
        app = create_app(conn)
        console.print(f"\n[bold green]Server running at http://localhost:{args.port}[/]\n")
        app.run(host="0.0.0.0", port=args.port, debug=False)
    else:
        # CLI mode
        from .documents import load_documents
        from .orchestrator import run_full_workflow, run_sequential

        docs = load_documents(args.docs)
        console.print(f"[cyan]Loaded {docs.file_count} files → {len(docs.chunks)} chunks[/]\n")

        async def run_cli():
            if args.mode == "sequential":
                gen = run_sequential(conn, docs, args.question)
            else:
                gen = run_full_workflow(conn, docs, args.question)

            async for evt in gen:
                if evt["type"] == "step_start":
                    console.print(f"\n[yellow]▶ {evt['agent']}:[/] {evt.get('description', '')}")
                elif evt["type"] == "step_done":
                    console.print(f"[green]✓ {evt['agent']}[/] ({evt.get('elapsed', '?')}s)")
                    console.print(evt.get("output", ""))
                elif evt["type"] == "complete":
                    console.print("\n[bold green]═══ Final Report ═══[/]")
                    console.print(evt.get("report", ""))
                elif evt["type"] == "error":
                    console.print(f"[red]✗ Error:[/] {evt.get('message', '')}")

        asyncio.run(run_cli())


if __name__ == "__main__":
    main()
