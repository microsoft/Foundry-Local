"""
Smoke Tests
────────────
Quick tests to verify imports, document loading, and bootstrapper configuration.
"""

from __future__ import annotations

import os
from pathlib import Path

import pytest


def test_imports():
    """All core modules can be imported."""
    from src.app.agents import create_planner, create_retriever, create_critic, create_writer, create_tool_agent
    from src.app.documents import load_documents, LoadedDocuments
    from src.app.foundry_boot import FoundryLocalBootstrapper, FoundryConnection
    from src.app.orchestrator import run_sequential, run_full_workflow
    from src.app.web import create_app


def test_document_loader():
    """Document loader reads data/ folder and produces chunks."""
    from src.app.documents import load_documents

    data_dir = Path(__file__).resolve().parent.parent / "data"
    if not data_dir.is_dir():
        pytest.skip("data/ directory not found")

    docs = load_documents(str(data_dir))
    assert docs.file_count > 0, "Expected at least one document file"
    assert len(docs.chunks) > 0, "Expected at least one chunk"
    assert len(docs.combined_text) > 0, "Expected non-empty combined text"


def test_document_loader_missing_dir():
    """Document loader returns empty result for missing directory."""
    from src.app.documents import load_documents

    docs = load_documents("/nonexistent/path/nothing/here")
    assert docs.file_count == 0
    assert len(docs.chunks) == 0


def test_foundry_connection_dataclass():
    """FoundryConnection stores fields correctly."""
    from src.app.foundry_boot import FoundryConnection

    conn = FoundryConnection(
        endpoint="http://localhost:5273",
        api_key="test-key",
        model_id="phi-4-mini-onnx-cpu",
        model_alias="phi-4-mini",
    )
    assert conn.endpoint == "http://localhost:5273"
    assert conn.model_alias == "phi-4-mini"


def test_bootstrapper_uses_env_override(monkeypatch):
    """Bootstrapper returns external endpoint when FOUNDRY_ENDPOINT is set."""
    from src.app.foundry_boot import FoundryLocalBootstrapper

    monkeypatch.setenv("FOUNDRY_ENDPOINT", "http://remote:8080/v1")
    monkeypatch.setenv("FOUNDRY_API_KEY", "my-key")

    boot = FoundryLocalBootstrapper(alias="test-model")
    conn = boot.bootstrap()

    assert conn.endpoint == "http://remote:8080/v1"
    assert conn.api_key == "my-key"
    assert conn.model_alias == "test-model"


def test_demo_registry():
    """Demo registry imports and has demos registered."""
    from src.app.demos.registry import list_demos, DEMO_REGISTRY

    # Import demo modules to trigger registration
    import src.app.demos.weather_tools
    import src.app.demos.math_agent
    import src.app.demos.sentiment_analyzer
    import src.app.demos.code_reviewer
    import src.app.demos.multi_agent_debate

    demos = list_demos()
    assert len(demos) >= 1, "Expected at least one registered demo"
