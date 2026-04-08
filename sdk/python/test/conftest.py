# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Shared test configuration and fixtures for Foundry Local Python SDK tests.

NOTE: "conftest.py" is a special filename that pytest uses to auto-discover
fixtures and shared utilities. All fixtures defined here are automatically
available to every test file without needing an explicit import.
This serves the same role as testUtils.ts in the JS SDK.
"""

from __future__ import annotations

import os
import logging
import sys

# ---------------------------------------------------------------------------
# Pre-load ORT/GenAI DLLs from e2e-test-pkgs BEFORE importing the SDK.
#
# The ``_brotli`` C extension (pulled in by httpx → openai → SDK) calls
# ``SetDefaultDllDirectories`` during import, which restricts subsequent
# ``LoadLibraryExW`` calls. Pre-loading here ensures ORT/GenAI are already
# in the process before brotli changes the DLL search behavior.
# ---------------------------------------------------------------------------
if sys.platform.startswith("win"):
    import ctypes
    from pathlib import Path as _Path

    def _preload_e2e_dlls():
        current = _Path(__file__).resolve().parent
        while True:
            candidate = current / "samples" / "python" / "e2e-test-pkgs"
            if candidate.exists():
                pkgs = candidate
                break
            parent = current.parent
            if parent == current:
                return
            current = parent

        ort_dll = pkgs / "onnxruntime.dll"
        genai_dll = pkgs / "onnxruntime-genai.dll"
        if not (ort_dll.exists() and genai_dll.exists()):
            return

        kernel32 = ctypes.windll.kernel32
        kernel32.SetDllDirectoryW(str(pkgs))
        os.add_dll_directory(str(pkgs))
        os.environ["ORT_LIB_PATH"] = str(ort_dll)

        kernel32.LoadLibraryExW.restype = ctypes.c_void_p
        kernel32.LoadLibraryExW.argtypes = [ctypes.c_wchar_p, ctypes.c_void_p, ctypes.c_int]
        kernel32.LoadLibraryExW(str(ort_dll), None, 0x00000008)
        kernel32.LoadLibraryExW(str(genai_dll), None, 0x00000008)

    _preload_e2e_dlls()

import pytest

from pathlib import Path

from foundry_local_sdk.configuration import Configuration, LogLevel
from foundry_local_sdk.foundry_local_manager import FoundryLocalManager

logger = logging.getLogger(__name__)

TEST_MODEL_ALIAS = "qwen2.5-0.5b"
AUDIO_MODEL_ALIAS = "whisper-tiny"

def get_git_repo_root() -> Path:
    """Walk upward from __file__ until we find a .git directory."""
    current = Path(__file__).resolve().parent
    while True:
        if (current / ".git").exists():
            return current
        parent = current.parent
        if parent == current:
            raise RuntimeError("Could not find git repo root")
        current = parent


def get_test_data_shared_path() -> str:
    """Return absolute path to the test-data-shared folder (sibling of the repo root)."""
    repo_root = get_git_repo_root()
    return str(repo_root.parent / "test-data-shared")


def is_running_in_ci() -> bool:
    """Check TF_BUILD (Azure DevOps) and GITHUB_ACTIONS env vars."""
    azure_devops = os.environ.get("TF_BUILD", "false").lower() == "true"
    github_actions = os.environ.get("GITHUB_ACTIONS", "false").lower() == "true"
    return azure_devops or github_actions


IS_RUNNING_IN_CI = is_running_in_ci()

skip_in_ci = pytest.mark.skipif(IS_RUNNING_IN_CI, reason="Skipped in CI environments")


def get_test_config() -> Configuration:
    """Build a Configuration suitable for integration tests."""
    repo_root = get_git_repo_root()
    return Configuration(
        app_name="FoundryLocalTest",
        model_cache_dir=get_test_data_shared_path(),
        log_level=LogLevel.WARNING,
        logs_dir=str(repo_root / "sdk" / "python" / "logs"),
        additional_settings={"Bootstrap": "false"},
    )


def get_multiply_tool():
    """Tool definition for the multiply_numbers function-calling test."""
    return {
        "type": "function",
        "function": {
            "name": "multiply_numbers",
            "description": "A tool for multiplying two numbers.",
            "parameters": {
                "type": "object",
                "properties": {
                    "first": {
                        "type": "integer",
                        "description": "The first number in the operation",
                    },
                    "second": {
                        "type": "integer",
                        "description": "The second number in the operation",
                    },
                },
                "required": ["first", "second"],
            },
        },
    }


# ---------------------------------------------------------------------------
# Session-scoped fixtures
# ---------------------------------------------------------------------------

@pytest.fixture(scope="session")
def manager():
    """Initialize FoundryLocalManager once for the entire test session."""
    # Reset singleton in case a previous run left state
    FoundryLocalManager.instance = None

    config = get_test_config()
    FoundryLocalManager.initialize(config)
    mgr = FoundryLocalManager.instance
    assert mgr is not None, "FoundryLocalManager.initialize did not set instance"

    yield mgr

    # Teardown: unload all loaded models
    try:
        catalog = mgr.catalog
        loaded = catalog.get_loaded_models()
        for model_variant in loaded:
            try:
                model_variant.unload()
            except Exception as e:
                logger.warning("Failed to unload model %s during teardown: %s", model_variant.id, e)
    except Exception as e:
        logger.warning("Failed to get loaded models during teardown: %s", e)

    # Reset the singleton so that other test sessions start clean
    FoundryLocalManager.instance = None


@pytest.fixture(scope="session")
def catalog(manager):
    """Return the Catalog from the session-scoped manager."""
    return manager.catalog


@pytest.fixture(scope="session")
def core_interop(manager):
    """Return the CoreInterop from the session-scoped manager (internal, for component tests)."""
    return manager._core_interop


@pytest.fixture(scope="session")
def model_load_manager(manager):
    """Return the ModelLoadManager from the session-scoped manager (internal, for component tests)."""
    return manager._model_load_manager
