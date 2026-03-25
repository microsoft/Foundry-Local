"""
Demo: Code Reviewer
───────────────────
Demonstrates code analysis tools for reviewing code snippets.
"""

from __future__ import annotations

import re
from typing import Annotated

from agent_framework import ChatAgent
from agent_framework.openai import OpenAIChatClient
from pydantic import Field

from ..foundry_boot import FoundryConnection
from .registry import DemoInfo, register_demo


# ─── Tool Functions ──────────────────────────────────────────────

def check_code_style(
    code: Annotated[str, Field(description="Code snippet to check for style issues")],
    language: Annotated[str, Field(description="Programming language (python, javascript, etc.)")] = "python",
) -> str:
    """Check code for common style issues."""
    issues = []
    lines = code.split("\n")
    for i, line in enumerate(lines, 1):
        if len(line) > 100:
            issues.append(f"Line {i}: Line too long ({len(line)} chars, max 100)")
        if line != line.rstrip():
            issues.append(f"Line {i}: Trailing whitespace")
        if "\t" in line and "    " in line:
            issues.append(f"Line {i}: Mixed tabs and spaces")

    if language.lower() == "python":
        if "import *" in code:
            issues.append("Avoid 'import *' \u2014 use explicit imports")
        if re.search(r"except\s*:", code):
            issues.append("Avoid bare 'except:' \u2014 catch specific exceptions")
        if re.search(r"def\s+\w+\([^)]*=\s*\[\]", code) or re.search(r"def\s+\w+\([^)]*=\s*\{\}", code):
            issues.append("Mutable default argument detected \u2014 use None instead")

    if language.lower() == "javascript":
        if "var " in code:
            issues.append("Consider using 'let' or 'const' instead of 'var'")
        if "==" in code and "===" not in code:
            issues.append("Consider using '===' for strict equality")

    if not issues:
        return "\u2705 No style issues found!"
    return "Style issues found:\n  \u2022 " + "\n  \u2022 ".join(issues)


def analyze_complexity(
    code: Annotated[str, Field(description="Code snippet to analyze")],
) -> str:
    """Analyze code complexity metrics."""
    lines = code.split("\n")
    total_lines = len(lines)
    code_lines = sum(1 for line in lines if line.strip() and not line.strip().startswith("#"))
    comment_lines = sum(1 for line in lines if line.strip().startswith("#"))
    blank_lines = sum(1 for line in lines if not line.strip())

    func_pattern = r"(?:def|function|async function|const\s+\w+\s*=\s*(?:async\s*)?\()"
    functions = len(re.findall(func_pattern, code))
    classes = len(re.findall(r"class\s+\w+", code))

    control_keywords = ["if", "elif", "else", "for", "while", "try", "except", "with", "case", "switch"]
    branches = sum(len(re.findall(rf"\b{kw}\b", code)) for kw in control_keywords)

    if branches <= 5:
        complexity = "Low"
    elif branches <= 15:
        complexity = "Medium"
    else:
        complexity = "High"

    return (
        f"Code Complexity Analysis:\n"
        f"  Lines: {total_lines} total ({code_lines} code, {comment_lines} comments, {blank_lines} blank)\n"
        f"  Functions/Methods: {functions}\n"
        f"  Classes: {classes}\n"
        f"  Branches: {branches}\n"
        f"  Estimated complexity: {complexity}"
    )


def find_potential_bugs(
    code: Annotated[str, Field(description="Code snippet to scan for potential bugs")],
) -> str:
    """Scan code for potential bugs and issues."""
    warnings = []
    if re.search(r"==\s*None", code):
        warnings.append("Use 'is None' instead of '== None'")
    if re.search(r"!=\s*None", code):
        warnings.append("Use 'is not None' instead of '!= None'")
    if re.search(r"print\s*\(", code):
        warnings.append("Debug print statement found \u2014 remove before production")
    if re.search(r"TODO|FIXME|HACK|XXX", code, re.IGNORECASE):
        warnings.append("TODO/FIXME comment found \u2014 address before release")
    if re.search(r"password\s*=\s*[\"'][^\"']+[\"']", code, re.IGNORECASE):
        warnings.append("\u26a0\ufe0f CRITICAL: Hardcoded password detected!")
    if re.search(r"api[_-]?key\s*=\s*[\"'][^\"']+[\"']", code, re.IGNORECASE):
        warnings.append("\u26a0\ufe0f CRITICAL: Hardcoded API key detected!")
    if "eval(" in code:
        warnings.append("\u26a0\ufe0f eval() is dangerous \u2014 avoid if possible")
    if "exec(" in code:
        warnings.append("\u26a0\ufe0f exec() is dangerous \u2014 avoid if possible")
    if re.search(r"except[^:]*:\s*\n\s*pass", code):
        warnings.append("Empty except block found \u2014 handle or log the exception")
    if not warnings:
        return "\u2705 No obvious bugs or issues detected!"
    return "Potential issues found:\n  \u2022 " + "\n  \u2022 ".join(warnings)


def suggest_improvements(
    code: Annotated[str, Field(description="Code snippet to review for improvements")],
) -> str:
    """Suggest code improvements and best practices."""
    suggestions = []
    func_pattern = r"def\s+\w+\s*\([^)]*\):\s*\n((?:\s+.*\n)*)"
    for match in re.finditer(func_pattern, code):
        body = match.group(1)
        if body.count("\n") > 30:
            suggestions.append("Consider breaking long functions into smaller ones (>30 lines)")
            break
    if re.search(r"def\s+\w+", code) and not re.search(r'""".*?"""', code, re.DOTALL):
        suggestions.append("Add docstrings to functions for better documentation")
    if re.search(r"def\s+\w+\s*\([^)]+\)", code):
        if not re.search(r"def\s+\w+\s*\([^)]*:\s*\w+", code):
            suggestions.append("Consider adding type hints for better code clarity")
    if re.search(r"[=<>+\-*/]\s*\d{2,}(?!\d)", code):
        suggestions.append("Extract magic numbers into named constants")
    long_lines = sum(1 for line in code.split("\n") if len(line) > 80)
    if long_lines > 3:
        suggestions.append(f"Break up {long_lines} long lines for better readability")
    if re.search(r"\b[a-z]\s*=", code):
        suggestions.append("Use descriptive variable names instead of single letters")
    if not suggestions:
        return "\u2705 Code looks good! No major improvements suggested."
    return "Suggested improvements:\n  \u2022 " + "\n  \u2022 ".join(suggestions)


def count_elements(
    code: Annotated[str, Field(description="Code snippet to analyze")],
) -> str:
    """Count code elements like variables, functions, loops, etc."""
    elements = {
        "variables": len(re.findall(r"\b\w+\s*=\s*(?!=)", code)),
        "functions": len(re.findall(r"\bdef\s+\w+", code)),
        "classes": len(re.findall(r"\bclass\s+\w+", code)),
        "if_statements": len(re.findall(r"\bif\s+", code)),
        "for_loops": len(re.findall(r"\bfor\s+", code)),
        "while_loops": len(re.findall(r"\bwhile\s+", code)),
        "try_blocks": len(re.findall(r"\btry\s*:", code)),
        "imports": len(re.findall(r"\bimport\s+", code)),
        "returns": len(re.findall(r"\breturn\s+", code)),
        "comments": len(re.findall(r"#.*$", code, re.MULTILINE)),
    }
    lines = ["Code element count:"]
    for element, count in elements.items():
        if count > 0:
            lines.append(f"  {element.replace('_', ' ').title()}: {count}")
    return "\n".join(lines)


# ─── Demo Class ──────────────────────────────────────────────────

class CodeReviewerDemo:
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
            name="CodeReviewer",
            instructions=(
                "You are a code review assistant. Use the provided tools to analyze code:\n\n"
                "  \u2022 check_code_style: Check for style issues\n"
                "  \u2022 analyze_complexity: Get complexity metrics\n"
                "  \u2022 find_potential_bugs: Scan for bugs and issues\n"
                "  \u2022 suggest_improvements: Get improvement suggestions\n"
                "  \u2022 count_elements: Count code elements\n\n"
                "When given code to review, use multiple tools to provide a comprehensive "
                "review. Summarize your findings clearly."
            ),
            tools=[check_code_style, analyze_complexity, find_potential_bugs, suggest_improvements, count_elements],
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
            "tools_available": ["check_code_style", "analyze_complexity", "find_potential_bugs", "suggest_improvements", "count_elements"],
        }


# ─── Register ────────────────────────────────────────────────────

async def run_code_review_demo(conn: FoundryConnection, prompt: str) -> dict:
    demo = CodeReviewerDemo(conn)
    return await demo.run(prompt)


register_demo(DemoInfo(
    id="code_reviewer",
    name="Code Reviewer",
    description="Code analysis agent that checks style, complexity, potential bugs, and suggests improvements.",
    icon="👨‍💻",
    category="Tool Calling",
    runner=run_code_review_demo,
    tags=["tools", "function-calling", "code-analysis", "single-agent"],
    suggested_prompt="Review this Python code:\n\ndef calc(x,y,z):\n    result = x + y\n    if result == None:\n        return 0\n    return result / z",
))
