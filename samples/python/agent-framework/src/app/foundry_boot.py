"""
Foundry Local Bootstrapper
───────────────────────────
Manages Foundry Local service lifecycle: starts the service,
checks/downloads/loads the model, and returns connection info.
"""

from __future__ import annotations

import logging
import os
from dataclasses import dataclass

from dotenv import load_dotenv
from foundry_local import FoundryLocalManager
from rich.console import Console

load_dotenv()
log = logging.getLogger(__name__)
console = Console()


@dataclass
class FoundryConnection:
    """Connection details returned after bootstrap."""
    endpoint: str
    api_key: str
    model_id: str
    model_alias: str


class FoundryLocalBootstrapper:
    """Bootstrap Foundry Local: start service → resolve model → download → load."""

    def __init__(self, alias: str | None = None):
        self.alias = alias or os.getenv("MODEL_ALIAS", "qwen2.5-0.5b")

    def bootstrap(self) -> FoundryConnection:
        """Start Foundry Local and return a ready-to-use connection."""
        endpoint_override = os.getenv("FOUNDRY_ENDPOINT")

        if endpoint_override:
            # External endpoint provided — skip local bootstrap
            console.print(f"[cyan]Using external endpoint:[/] {endpoint_override}")
            return FoundryConnection(
                endpoint=endpoint_override,
                api_key=os.getenv("FOUNDRY_API_KEY", "none"),
                model_id=self.alias,
                model_alias=self.alias,
            )

        console.print(f"[cyan]Bootstrapping Foundry Local with alias:[/] {self.alias}")

        # FoundryLocalManager(alias) auto-starts service + resolves model
        manager = FoundryLocalManager(self.alias)

        endpoint = manager.endpoint
        api_key = manager.api_key

        # List cached models to find the resolved variant
        cached = manager.list_cached_models()
        model_id = self.alias
        for m in cached:
            if self.alias in str(m):
                model_id = str(m)
                break

        console.print(f"[green]✓ Foundry Local ready[/]  endpoint={endpoint}")
        log.info("Foundry Local ready: endpoint=%s model=%s", endpoint, model_id)

        return FoundryConnection(
            endpoint=endpoint,
            api_key=api_key,
            model_id=model_id,
            model_alias=self.alias,
        )
