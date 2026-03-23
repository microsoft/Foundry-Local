"""
Demo Registry
─────────────
Central registry of all available demos with metadata.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Callable


@dataclass
class DemoInfo:
    """Metadata for a demo."""
    id: str
    name: str
    description: str
    icon: str
    category: str
    runner: Callable[..., Any]
    tags: list[str]
    suggested_prompt: str = ""


# Registry populated by each demo module
DEMO_REGISTRY: dict[str, DemoInfo] = {}


def register_demo(info: DemoInfo) -> None:
    """Register a demo in the global registry."""
    DEMO_REGISTRY[info.id] = info


def get_demo(demo_id: str) -> DemoInfo | None:
    """Get demo info by ID."""
    return DEMO_REGISTRY.get(demo_id)


def list_demos() -> list[DemoInfo]:
    """List all demos with their metadata."""
    return list(DEMO_REGISTRY.values())
