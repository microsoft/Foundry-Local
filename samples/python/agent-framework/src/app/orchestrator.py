"""
Orchestrator
─────────────
Three orchestration patterns for the multi-agent workflow:
  1. Sequential   — Planner → Retriever → Critic ⇄ Retriever → Writer
  2. Concurrent   — Retriever ‖ ToolAgent (fan-out)
  3. Full         — Combines sequential + concurrent
"""

from __future__ import annotations

import asyncio
import logging
import re
import time
from typing import AsyncGenerator

from .agents import (
    create_critic,
    create_planner,
    create_retriever,
    create_tool_agent,
    create_writer,
)
from .documents import LoadedDocuments
from .foundry_boot import FoundryConnection

log = logging.getLogger(__name__)

MAX_CRITIC_LOOPS = 2


def _critic_found_gaps(critique: str) -> bool:
    """Return True if the critic found gaps (i.e. didn't say NO_GAPS_FOUND)."""
    return "NO_GAPS_FOUND" not in critique.upper().replace(" ", "")


# ─── Streaming helpers ───────────────────────────────────────────

StepEvent = dict  # {"type": str, ...}


async def run_sequential(
    conn: FoundryConnection,
    docs: LoadedDocuments,
    question: str,
) -> AsyncGenerator[StepEvent, None]:
    """Sequential pipeline: Planner → Retriever → Critic → Writer."""

    planner = create_planner(conn)
    retriever = create_retriever(conn, docs.combined_text)
    critic = create_critic(conn)
    writer = create_writer(conn)

    # ── Planner ──
    yield {"type": "step_start", "agent": "Planner", "description": "Breaking question into sub-tasks"}
    t0 = time.perf_counter()
    plan = await planner.run(question)
    plan_text = str(plan)
    elapsed = round(time.perf_counter() - t0, 2)
    yield {"type": "step_done", "agent": "Planner", "output": plan_text, "elapsed": elapsed}

    # ── Retriever ──
    yield {"type": "step_start", "agent": "Retriever", "description": "Searching documents"}
    t0 = time.perf_counter()
    snippets = await retriever.run(plan_text)
    snippets_text = str(snippets)
    elapsed = round(time.perf_counter() - t0, 2)
    yield {"type": "step_done", "agent": "Retriever", "output": snippets_text, "elapsed": elapsed}

    # ── Critic loop ──
    combined = f"Plan:\n{plan_text}\n\nRetrieved:\n{snippets_text}"
    for loop in range(MAX_CRITIC_LOOPS):
        yield {"type": "step_start", "agent": "Critic", "description": f"Reviewing for gaps (round {loop + 1})"}
        t0 = time.perf_counter()
        critique = await critic.run(combined)
        critique_text = str(critique)
        elapsed = round(time.perf_counter() - t0, 2)
        yield {"type": "step_done", "agent": "Critic", "output": critique_text, "elapsed": elapsed}

        if not _critic_found_gaps(critique_text):
            break

        # Re-retrieve with critic feedback
        yield {"type": "step_start", "agent": "Retriever", "description": "Re-searching based on critic feedback"}
        t0 = time.perf_counter()
        snippets = await retriever.run(f"{plan_text}\n\nCritic feedback:\n{critique_text}")
        snippets_text = str(snippets)
        elapsed = round(time.perf_counter() - t0, 2)
        yield {"type": "step_done", "agent": "Retriever", "output": snippets_text, "elapsed": elapsed}

        combined = f"Plan:\n{plan_text}\n\nRetrieved:\n{snippets_text}\n\nCritique:\n{critique_text}"

    # ── Writer ──
    yield {"type": "step_start", "agent": "Writer", "description": "Synthesising final report"}
    t0 = time.perf_counter()
    report = await writer.run(combined)
    report_text = str(report)
    elapsed = round(time.perf_counter() - t0, 2)
    yield {"type": "step_done", "agent": "Writer", "output": report_text, "elapsed": elapsed}

    yield {"type": "complete", "report": report_text}


async def run_concurrent_retrieval(
    conn: FoundryConnection,
    docs: LoadedDocuments,
    plan_text: str,
) -> AsyncGenerator[StepEvent, None]:
    """Concurrent fan-out: Retriever ‖ ToolAgent on the same plan."""

    retriever = create_retriever(conn, docs.combined_text)
    tool_agent = create_tool_agent(conn)

    yield {"type": "step_start", "agent": "Concurrent", "description": "Retriever + ToolAgent in parallel"}
    t0 = time.perf_counter()
    snippets_task = retriever.run(plan_text)
    keywords_task = tool_agent.run(f"Analyze this text:\n{docs.combined_text[:3000]}")
    snippets, keywords = await asyncio.gather(snippets_task, keywords_task)
    elapsed = round(time.perf_counter() - t0, 2)

    yield {
        "type": "step_done",
        "agent": "Concurrent",
        "output": f"**Retriever:**\n{snippets}\n\n**ToolAgent:**\n{keywords}",
        "elapsed": elapsed,
    }


async def run_full_workflow(
    conn: FoundryConnection,
    docs: LoadedDocuments,
    question: str,
) -> AsyncGenerator[StepEvent, None]:
    """Full hybrid: Sequential plan → Concurrent retrieve → Sequential critique + write."""

    planner = create_planner(conn)
    critic = create_critic(conn)
    writer = create_writer(conn)

    # ── Planner (sequential) ──
    yield {"type": "step_start", "agent": "Planner", "description": "Breaking question into sub-tasks"}
    t0 = time.perf_counter()
    plan = await planner.run(question)
    plan_text = str(plan)
    elapsed = round(time.perf_counter() - t0, 2)
    yield {"type": "step_done", "agent": "Planner", "output": plan_text, "elapsed": elapsed}

    # ── Concurrent fan-out ──
    snippets_text = ""
    keywords_text = ""
    async for evt in run_concurrent_retrieval(conn, docs, plan_text):
        yield evt
        if evt["type"] == "step_done" and evt["agent"] == "Concurrent":
            # Parse out retriever/tool output
            output = evt.get("output", "")
            snippets_text = output
            keywords_text = ""

    # ── Critic (sequential) ──
    combined = f"Plan:\n{plan_text}\n\nRetrieved + Keywords:\n{snippets_text}"
    for loop in range(MAX_CRITIC_LOOPS):
        yield {"type": "step_start", "agent": "Critic", "description": f"Reviewing for gaps (round {loop + 1})"}
        t0 = time.perf_counter()
        critique = await critic.run(combined)
        critique_text = str(critique)
        elapsed = round(time.perf_counter() - t0, 2)
        yield {"type": "step_done", "agent": "Critic", "output": critique_text, "elapsed": elapsed}

        if not _critic_found_gaps(critique_text):
            break
        combined += f"\n\nCritique:\n{critique_text}"

    # ── Writer (sequential) ──
    yield {"type": "step_start", "agent": "Writer", "description": "Synthesising final report"}
    t0 = time.perf_counter()
    report = await writer.run(combined)
    report_text = str(report)
    elapsed = round(time.perf_counter() - t0, 2)
    yield {"type": "step_done", "agent": "Writer", "output": report_text, "elapsed": elapsed}

    yield {"type": "complete", "report": report_text}
