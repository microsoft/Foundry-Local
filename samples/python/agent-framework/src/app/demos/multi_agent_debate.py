"""
Demo: Multi-Agent Debate
────────────────────────
Demonstrates multi-agent orchestration with opposing viewpoints.
Three agents debate a topic: Proponent, Opponent, and Moderator.
"""

from __future__ import annotations

import re
import time
from dataclasses import dataclass

from agent_framework import ChatAgent
from agent_framework.openai import OpenAIChatClient

from ..foundry_boot import FoundryConnection
from .registry import DemoInfo, register_demo


# ─── Debate Participants ─────────────────────────────────────────

def _make_client(conn: FoundryConnection) -> OpenAIChatClient:
    return OpenAIChatClient(
        api_key=conn.api_key,
        base_url=conn.endpoint,
        model_id=conn.model_id,
    )


def create_proponent(conn: FoundryConnection) -> ChatAgent:
    """Create agent that argues FOR the topic."""
    return ChatAgent(
        chat_client=_make_client(conn),
        name="Proponent",
        instructions=(
            "You are a skilled debater arguing IN FAVOR of the given topic.\n\n"
            "Rules:\n"
            "  \u2022 Present 2-3 strong arguments supporting the position\n"
            "  \u2022 Use logic, examples, and evidence\n"
            "  \u2022 Be persuasive but respectful\n"
            "  \u2022 Keep your response to 3-4 paragraphs max\n\n"
            "Start with: 'I argue IN FAVOR because...'"
        ),
    )


def create_opponent(conn: FoundryConnection) -> ChatAgent:
    """Create agent that argues AGAINST the topic."""
    return ChatAgent(
        chat_client=_make_client(conn),
        name="Opponent",
        instructions=(
            "You are a skilled debater arguing AGAINST the given topic.\n\n"
            "Rules:\n"
            "  \u2022 Present 2-3 strong counter-arguments\n"
            "  \u2022 Respond to the previous speaker's points where relevant\n"
            "  \u2022 Use logic, examples, and evidence\n"
            "  \u2022 Keep your response to 3-4 paragraphs max\n\n"
            "Start with: 'I argue AGAINST because...'"
        ),
    )


def create_moderator(conn: FoundryConnection) -> ChatAgent:
    """Create moderator agent that summarizes the debate."""
    return ChatAgent(
        chat_client=_make_client(conn),
        name="Moderator",
        instructions=(
            "You are an impartial debate moderator.\n\n"
            "Your job:\n"
            "  1. Summarize the key points from BOTH sides\n"
            "  2. Identify the strongest argument from each side\n"
            "  3. Declare which side presented a more compelling case\n"
            "  4. Explain your reasoning briefly\n\n"
            "Be fair and objective. Format:\n"
            "  \u2022 FOR side summary: ...\n"
            "  \u2022 AGAINST side summary: ...\n"
            "  \u2022 Verdict: [FOR/AGAINST] wins because..."
        ),
    )


# ─── Debate Results ──────────────────────────────────────────────

@dataclass
class DebateRound:
    speaker: str
    position: str
    argument: str
    elapsed: float


# ─── Demo Class ──────────────────────────────────────────────────

class DebateDemo:
    def __init__(self, conn: FoundryConnection):
        self.conn = conn
        self.proponent = create_proponent(conn)
        self.opponent = create_opponent(conn)
        self.moderator = create_moderator(conn)

    async def _run_agent(self, agent: ChatAgent, prompt: str) -> tuple[str, float]:
        t0 = time.perf_counter()
        result = await agent.run(prompt)
        elapsed = time.perf_counter() - t0
        text = re.sub(r"<tool_call>.*?</tool_call>\s*", "", str(result), flags=re.DOTALL).strip()
        return text, elapsed

    async def run(self, topic: str) -> dict:
        t0_total = time.perf_counter()
        rounds = []

        # Round 1: Proponent opens
        pro_prompt = f'Topic for debate: "{topic}"\n\nPresent your opening arguments IN FAVOR of this topic.'
        pro_argument, pro_time = await self._run_agent(self.proponent, pro_prompt)
        rounds.append(DebateRound("Proponent", "FOR", pro_argument, pro_time))

        # Round 2: Opponent responds
        opp_prompt = (
            f'Topic for debate: "{topic}"\n\n'
            f"The speaker FOR this topic argued:\n{pro_argument}\n\n"
            "Present your counter-arguments AGAINST this topic."
        )
        opp_argument, opp_time = await self._run_agent(self.opponent, opp_prompt)
        rounds.append(DebateRound("Opponent", "AGAINST", opp_argument, opp_time))

        # Final: Moderator verdict
        mod_prompt = (
            f'Topic for debate: "{topic}"\n\n'
            f"=== Arguments FOR ===\n{pro_argument}\n\n"
            f"=== Arguments AGAINST ===\n{opp_argument}\n\n"
            "Please summarize the debate and declare a winner."
        )
        verdict_text, mod_time = await self._run_agent(self.moderator, mod_prompt)
        rounds.append(DebateRound("Moderator", "VERDICT", verdict_text, mod_time))

        total_time = time.perf_counter() - t0_total

        # Extract verdict
        verdict = "TIE"
        if "for wins" in verdict_text.lower() or "proponent wins" in verdict_text.lower():
            verdict = "FOR"
        elif "against wins" in verdict_text.lower() or "opponent wins" in verdict_text.lower():
            verdict = "AGAINST"

        response_parts = []
        for r in rounds:
            response_parts.append(f"=== {r.speaker} ({r.position}) ===\n{r.argument}")
        response_text = "\n\n".join(response_parts)
        if verdict != "TIE":
            response_text += f"\n\nVerdict: {verdict}"

        return {
            "response": response_text,
            "topic": topic,
            "rounds": [
                {
                    "speaker": r.speaker,
                    "position": r.position,
                    "argument": r.argument,
                    "elapsed": round(r.elapsed, 2),
                }
                for r in rounds
            ],
            "verdict": verdict,
            "total_time": round(total_time, 2),
            "agents_used": ["Proponent (FOR)", "Opponent (AGAINST)", "Moderator"],
        }


# ─── Register ────────────────────────────────────────────────────

async def run_debate_demo(conn: FoundryConnection, prompt: str) -> dict:
    demo = DebateDemo(conn)
    return await demo.run(prompt)


register_demo(DemoInfo(
    id="multi_agent_debate",
    name="Multi-Agent Debate",
    description="Three agents debate a topic: one argues FOR, one argues AGAINST, and a moderator declares a winner.",
    icon="🎭",
    category="Multi-Agent",
    runner=run_debate_demo,
    tags=["multi-agent", "orchestration", "sequential", "debate"],
    suggested_prompt="Remote work should become the default for all knowledge workers",
))
