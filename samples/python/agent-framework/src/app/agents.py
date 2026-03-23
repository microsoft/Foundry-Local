"""
Agent Factories
────────────────
Create specialised ChatAgents for the multi-agent research workflow.
"""

from __future__ import annotations

import re
from typing import Annotated

from agent_framework import ChatAgent
from agent_framework.openai import OpenAIChatClient
from pydantic import Field

from .foundry_boot import FoundryConnection


# ─── Tool Functions ──────────────────────────────────────────────

def word_count(
    text: Annotated[str, Field(description="Text to count words in")],
) -> str:
    """Count the number of words in the given text."""
    count = len(text.split())
    return f"Word count: {count}"


def extract_keywords(
    text: Annotated[str, Field(description="Text to extract keywords from")],
) -> str:
    """Extract the most frequently repeated keywords from text."""
    words = re.findall(r"\b\w{4,}\b", text.lower())
    freq: dict[str, int] = {}
    for w in words:
        freq[w] = freq.get(w, 0) + 1
    repeated = {w: c for w, c in freq.items() if c >= 2}
    if not repeated:
        return "Keywords: (none detected)"
    top = sorted(repeated, key=repeated.get, reverse=True)[:10]
    return "Keywords: " + ", ".join(top)


# ─── Agent Factories ────────────────────────────────────────────

def _make_client(conn: FoundryConnection) -> OpenAIChatClient:
    return OpenAIChatClient(
        api_key=conn.api_key,
        base_url=conn.endpoint,
        model_id=conn.model_id,
    )


def create_planner(conn: FoundryConnection) -> ChatAgent:
    """Create the Planner agent that breaks a question into sub-tasks."""
    return ChatAgent(
        chat_client=_make_client(conn),
        name="Planner",
        instructions=(
            "You are a research planner. Given a user question, break it into "
            "2-4 concise sub-tasks that other agents can handle.\n"
            "Output a numbered list of sub-tasks, nothing else."
        ),
    )


def create_retriever(conn: FoundryConnection, documents_text: str) -> ChatAgent:
    """Create the Retriever agent that finds relevant snippets from documents."""
    return ChatAgent(
        chat_client=_make_client(conn),
        name="Retriever",
        instructions=(
            "You are a document retriever. Given sub-tasks, search the documents "
            "below and return relevant excerpts with [source: filename] citations.\n\n"
            "─── DOCUMENTS ───\n" + documents_text
        ),
    )


def create_critic(conn: FoundryConnection) -> ChatAgent:
    """Create the Critic agent that reviews output for gaps."""
    return ChatAgent(
        chat_client=_make_client(conn),
        name="Critic",
        instructions=(
            "You are a research critic. Review the plan AND retrieved snippets.\n"
            "List any gaps, contradictions, or missing sub-topics.\n"
            "If nothing is missing, respond with exactly: NO_GAPS_FOUND"
        ),
    )


def create_writer(conn: FoundryConnection) -> ChatAgent:
    """Create the Writer agent that produces the final report."""
    return ChatAgent(
        chat_client=_make_client(conn),
        name="Writer",
        instructions=(
            "You are a technical writer. Synthesize the plan, retrieved snippets, "
            "keywords, and critic feedback into a clear, well-structured report.\n"
            "Include [source: filename] citations where applicable."
        ),
    )


def create_tool_agent(conn: FoundryConnection) -> ChatAgent:
    """Create the ToolAgent that uses word_count and extract_keywords tools."""
    return ChatAgent(
        chat_client=_make_client(conn),
        name="ToolAgent",
        instructions=(
            "You are a text analysis agent. Use the provided tools to count words "
            "and extract keywords from the text you receive."
        ),
        tools=[word_count, extract_keywords],
    )
