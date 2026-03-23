"""
Demo: Math Agent
────────────────
Demonstrates calculation tools and step-by-step reasoning.
"""

from __future__ import annotations

import math
import re
from typing import Annotated

from agent_framework import ChatAgent
from agent_framework.openai import OpenAIChatClient
from pydantic import Field

from ..foundry_boot import FoundryConnection
from .registry import DemoInfo, register_demo


# ─── Tool Functions ──────────────────────────────────────────────

def calculate(
    expression: Annotated[str, Field(description="Math expression to evaluate, e.g. '(5 + 3) * 2'")],
) -> str:
    """Safely evaluate a mathematical expression."""
    allowed = set("0123456789+-*/.() ")
    if not all(c in allowed for c in expression):
        return f"Error: Expression contains invalid characters. Use only numbers and operators: + - * / ( )"
    try:
        result = eval(expression, {"__builtins__": {}}, {})  # noqa: S307
        if isinstance(result, float) and result.is_integer():
            result = int(result)
        return f"Result: {expression} = {result}"
    except Exception as e:
        return f"Error evaluating '{expression}': {e}"


def percentage(
    value: Annotated[float, Field(description="The value to calculate percentage of")],
    percent: Annotated[float, Field(description="The percentage to apply")],
) -> str:
    """Calculate a percentage of a value."""
    result = value * (percent / 100)
    return f"{percent}% of {value} = {result}"


def percentage_change(
    old_value: Annotated[float, Field(description="The original value")],
    new_value: Annotated[float, Field(description="The new value")],
) -> str:
    """Calculate the percentage change between two values."""
    if old_value == 0:
        return "Error: Cannot calculate percentage change from zero"
    change = ((new_value - old_value) / old_value) * 100
    direction = "increase" if change > 0 else "decrease" if change < 0 else "no change"
    return f"From {old_value} to {new_value}: {abs(change):.2f}% {direction}"


def convert_units(
    value: Annotated[float, Field(description="The value to convert")],
    from_unit: Annotated[str, Field(description="Source unit (e.g., 'km', 'miles', 'kg', 'lbs', 'celsius', 'fahrenheit')")],
    to_unit: Annotated[str, Field(description="Target unit")],
) -> str:
    """Convert between common units."""
    conversions = {
        ("km", "miles"): lambda x: x * 0.621371,
        ("miles", "km"): lambda x: x * 1.60934,
        ("kg", "lbs"): lambda x: x * 2.20462,
        ("lbs", "kg"): lambda x: x * 0.453592,
        ("celsius", "fahrenheit"): lambda x: (x * 9 / 5) + 32,
        ("fahrenheit", "celsius"): lambda x: (x - 32) * 5 / 9,
        ("meters", "feet"): lambda x: x * 3.28084,
        ("feet", "meters"): lambda x: x * 0.3048,
        ("liters", "gallons"): lambda x: x * 0.264172,
        ("gallons", "liters"): lambda x: x * 3.78541,
    }
    key = (from_unit.lower(), to_unit.lower())
    if key not in conversions:
        available = ", ".join(f"{f}\u2192{t}" for f, t in conversions)
        return f"Conversion not supported. Available: {available}"
    result = conversions[key](value)
    return f"{value} {from_unit} = {result:.4f} {to_unit}"


def compound_interest(
    principal: Annotated[float, Field(description="Initial investment amount")],
    rate: Annotated[float, Field(description="Annual interest rate as percentage (e.g., 5 for 5%)")],
    years: Annotated[int, Field(description="Number of years")],
    compounds_per_year: Annotated[int, Field(description="Times interest compounds per year (1=annual, 12=monthly, 365=daily)")] = 12,
) -> str:
    """Calculate compound interest."""
    r = rate / 100
    n = compounds_per_year
    t = years
    amount = principal * (1 + r / n) ** (n * t)
    interest = amount - principal
    return (
        f"Compound Interest Calculation:\n"
        f"  Principal: ${principal:,.2f}\n"
        f"  Rate: {rate}% per year\n"
        f"  Time: {years} years\n"
        f"  Compounds: {n}x per year\n"
        f"  \u2192 Final amount: ${amount:,.2f}\n"
        f"  \u2192 Interest earned: ${interest:,.2f}"
    )


def statistics(
    numbers: Annotated[str, Field(description="Comma-separated list of numbers, e.g., '1, 2, 3, 4, 5'")],
) -> str:
    """Calculate basic statistics for a list of numbers."""
    try:
        nums = [float(n.strip()) for n in numbers.split(",")]
    except ValueError:
        return "Error: Invalid number format. Use comma-separated numbers like '1, 2, 3'"
    if not nums:
        return "Error: No numbers provided"
    n = len(nums)
    mean = sum(nums) / n
    sorted_nums = sorted(nums)
    if n % 2 == 0:
        median = (sorted_nums[n // 2 - 1] + sorted_nums[n // 2]) / 2
    else:
        median = sorted_nums[n // 2]
    variance = sum((x - mean) ** 2 for x in nums) / n
    std_dev = math.sqrt(variance)
    return (
        f"Statistics for {n} numbers:\n"
        f"  Sum: {sum(nums)}\n"
        f"  Mean: {mean:.2f}\n"
        f"  Median: {median:.2f}\n"
        f"  Min: {min(nums)}\n"
        f"  Max: {max(nums)}\n"
        f"  Range: {max(nums) - min(nums)}\n"
        f"  Std Dev: {std_dev:.2f}"
    )


# ─── Demo Class ──────────────────────────────────────────────────

class MathAgentDemo:
    def __init__(self, conn: FoundryConnection):
        self.conn = conn
        self.agent = self._create_agent()

    def _create_agent(self) -> ChatAgent:
        client = OpenAIChatClient(
            api_key=self.conn.api_key,
            base_url=self.conn.endpoint,
            model_id=self.conn.model_id,
        )
        return ChatAgent(
            chat_client=client,
            name="MathAssistant",
            instructions=(
                "You are a precise math assistant. ALWAYS use the provided tools "
                "for calculations \u2014 never compute in your head.\n\n"
                "Available tools:\n"
                "  \u2022 calculate: Evaluate math expressions\n"
                "  \u2022 percentage: Calculate percentages\n"
                "  \u2022 percentage_change: Calculate % change between values\n"
                "  \u2022 convert_units: Convert between units\n"
                "  \u2022 compound_interest: Calculate investment growth\n"
                "  \u2022 statistics: Compute stats for a list of numbers\n\n"
                "Show your work by using tools step-by-step for complex problems."
            ),
            tools=[calculate, percentage, percentage_change, convert_units, compound_interest, statistics],
        )

    async def run(self, prompt: str) -> dict:
        import time
        t0 = time.perf_counter()
        result = await self.agent.run(prompt)
        elapsed = time.perf_counter() - t0
        text = re.sub(r"<tool_call>.*?</tool_call>\s*", "", str(result), flags=re.DOTALL).strip()
        return {
            "prompt": prompt,
            "response": text,
            "elapsed": round(elapsed, 2),
            "tools_available": ["calculate", "percentage", "percentage_change", "convert_units", "compound_interest", "statistics"],
        }


# ─── Register ────────────────────────────────────────────────────

async def run_math_demo(conn: FoundryConnection, prompt: str) -> dict:
    demo = MathAgentDemo(conn)
    return await demo.run(prompt)


register_demo(DemoInfo(
    id="math_agent",
    name="Math Calculator",
    description="Precise calculation agent with tools for arithmetic, percentages, unit conversions, compound interest, and statistics.",
    icon="\ud83d\udd22",
    category="Tool Calling",
    runner=run_math_demo,
    tags=["tools", "function-calling", "calculations", "single-agent"],
    suggested_prompt="If I invest $10,000 at 7% annual interest compounded monthly for 15 years, how much will I have? Also convert that to euros assuming 1 USD = 0.92 EUR.",
))
