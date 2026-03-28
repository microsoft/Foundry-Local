"""
Tool Demo
──────────
Standalone validation: run tool-calling with Foundry Local to verify
that both direct invocation and LLM-driven function calling work.
"""

from __future__ import annotations

import asyncio
import time

from agent_framework import ChatAgent
from agent_framework.openai import OpenAIChatClient
from rich.console import Console

from .agents import extract_keywords, word_count
from .foundry_boot import FoundryConnection

console = Console()


async def run_tool_demo(conn: FoundryConnection) -> list[dict]:
    """Run direct + LLM-driven tool tests and return results."""
    results: list[dict] = []

    # ── Direct function calls ────────────────────────────
    t0 = time.perf_counter()
    wc = word_count("Foundry Local runs models on device")
    results.append({
        "test": "Direct: word_count",
        "result": wc,
        "status": "pass" if "6" in wc else "fail",
        "elapsed": round(time.perf_counter() - t0, 4),
    })

    t0 = time.perf_counter()
    kw = extract_keywords("foundry foundry local local model model agent")
    results.append({
        "test": "Direct: extract_keywords",
        "result": kw,
        "status": "pass" if "foundry" in kw.lower() else "fail",
        "elapsed": round(time.perf_counter() - t0, 4),
    })

    # ── LLM-driven tool call ─────────────────────────────
    client = OpenAIChatClient(
        api_key=conn.api_key,
        base_url=conn.endpoint,
        model_id=conn.model_id,
    )
    agent = ChatAgent(
        chat_client=client,
        name="ToolTester",
        instructions="Use the provided tools to answer.",
        tools=[word_count, extract_keywords],
    )

    t0 = time.perf_counter()
    try:
        result = await agent.run("Count the words in: 'hello world from foundry local'")
        result_text = str(result)
        results.append({
            "test": "LLM: word_count via agent",
            "result": result_text,
            "status": "pass" if any(c.isdigit() for c in result_text) else "fail",
            "elapsed": round(time.perf_counter() - t0, 2),
        })
    except Exception as exc:
        results.append({
            "test": "LLM: word_count via agent",
            "result": str(exc),
            "status": "fail",
            "elapsed": round(time.perf_counter() - t0, 2),
        })

    t0 = time.perf_counter()
    try:
        result = await agent.run(
            "Extract keywords from: 'foundry foundry local local model model inference inference'"
        )
        result_text = str(result)
        results.append({
            "test": "LLM: extract_keywords via agent",
            "result": result_text,
            "status": "pass" if "foundry" in result_text.lower() or "keyword" in result_text.lower() else "fail",
            "elapsed": round(time.perf_counter() - t0, 2),
        })
    except Exception as exc:
        results.append({
            "test": "LLM: extract_keywords via agent",
            "result": str(exc),
            "status": "fail",
            "elapsed": round(time.perf_counter() - t0, 2),
        })

    return results
