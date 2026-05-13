# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Shared fixtures for the Foundry Local Python SDK test suite.

CI policy mirrors the C++ integration tests (see
``sdk_v2/cpp/test/sdk_api/shared_test_env.h``):

- ``IS_CI`` is true when ``TF_BUILD=true`` (Azure DevOps) or
  ``GITHUB_ACTIONS=true`` (GitHub Actions), case-insensitive.
- Tests that require a model **never download** in CI. They use a model
  only when it is already present in the local model cache. When no
  suitable model is cached, the test is skipped with a clear reason.
- Locally the same rule applies: pre-cache models with the Foundry
  Local CLI before running model-dependent tests.

The ``Manager`` is a singleton — it is created exactly once per test
process by the session-scoped ``manager`` fixture.
"""

from __future__ import annotations

import os

import pytest


# ---------------------------------------------------------------------------
# CI detection
# ---------------------------------------------------------------------------

def is_running_in_ci() -> bool:
    azure_devops = os.environ.get("TF_BUILD", "false").lower() == "true"
    github_actions = os.environ.get("GITHUB_ACTIONS", "false").lower() == "true"
    return azure_devops or github_actions


IS_CI: bool = is_running_in_ci()

# Optional override that points the SDK at a pre-staged model cache so
# integration tests can find cached models without downloading.
# Mirrors the C++ TEST_MODEL_CACHE_DIR env var.
TEST_MODEL_CACHE_DIR: str | None = os.environ.get("TEST_MODEL_CACHE_DIR") or None


# ---------------------------------------------------------------------------
# Session-scoped manager — singleton, created exactly once per test process
# ---------------------------------------------------------------------------

@pytest.fixture(scope="session")
def manager():
    """Initialize the FoundryLocalManager singleton for the test session.

    Skips the entire integration suite cleanly when the native library
    cannot be loaded (e.g. wheel built without the .pyd).
    """
    try:
        from foundry_local_sdk import Configuration, FoundryLocalManager, LogLevel
    except ImportError as e:
        pytest.skip(f"foundry_local_sdk package not importable: {e}")

    # If the singleton already exists (another fixture in this session
    # already initialised it), reuse it.
    if FoundryLocalManager.instance is not None:
        return FoundryLocalManager.instance

    config_kwargs = {
        "app_name": "FoundryLocalPythonTests",
        "log_level": LogLevel.WARNING,
    }
    if TEST_MODEL_CACHE_DIR:
        config_kwargs["model_cache_dir"] = TEST_MODEL_CACHE_DIR

    try:
        config = Configuration(**config_kwargs)
        FoundryLocalManager.initialize(config)
    except Exception as e:
        pytest.skip(f"FoundryLocalManager could not be initialized: {e}")

    return FoundryLocalManager.instance


# ---------------------------------------------------------------------------
# Model discovery helpers
# ---------------------------------------------------------------------------

def _find_smallest_cached_model_for_task(manager, task: str):
    """Return the smallest cached model variant whose task matches.

    Walks each alias and its variants — selecting a CPU variant when the
    default selection has the wrong task — to mirror the C++ helper
    ``FindSmallestModelByTask``.
    """
    best = None
    best_size = None
    for model in manager.catalog.list_models():
        info = model.info
        if info.task != task:
            # Try variants for a matching task.
            try:
                variants = model.variants
            except Exception:
                variants = []
            matched = None
            for v in variants:
                if v.info.task == task:
                    matched = v
                    break
            if matched is None:
                continue
            try:
                model.select_variant(matched)
                info = model.info
            except Exception:
                continue
        if not model.is_cached:
            continue
        size = info.file_size_mb or 0
        if size <= 0:
            continue
        if best_size is None or size < best_size:
            best = model
            best_size = size
    return best


def _model_fixture_or_skip(manager, task: str, role: str, *, load: bool):
    model = _find_smallest_cached_model_for_task(manager, task)
    if model is None:
        reason = (
            f"No cached {role} model (task={task!r}) available. "
            f"In CI: pre-stage one in TEST_MODEL_CACHE_DIR. "
            f"Locally: run 'foundry model download <alias>' first."
        )
        pytest.skip(reason)
    if load:
        try:
            model.load()
        except Exception as e:
            pytest.skip(f"Could not load {role} model {model.alias!r}: {e}")
    return model


@pytest.fixture(scope="session")
def chat_model(manager):
    """Smallest cached chat-completion model, loaded. Skips if none cached."""
    return _model_fixture_or_skip(manager, "chat-completion", "chat", load=True)


@pytest.fixture(scope="session")
def embedding_model(manager):
    """Smallest cached embeddings model, loaded. Skips if none cached."""
    return _model_fixture_or_skip(manager, "embeddings", "embedding", load=True)


@pytest.fixture(scope="session")
def audio_model(manager):
    """Smallest cached ASR model, loaded. Skips if none cached."""
    return _model_fixture_or_skip(
        manager, "automatic-speech-recognition", "audio", load=True
    )


# ---------------------------------------------------------------------------
# Native-layer availability — used to skip integration tests if the .pyd
# was not built or is incompatible with the current Python.
# ---------------------------------------------------------------------------

@pytest.fixture(scope="session")
def native_api():
    try:
        from foundry_local_sdk._native.api import api, ffi  # noqa: F401
    except Exception as e:
        pytest.skip(f"native cffi extension not loadable: {e}")
    from foundry_local_sdk._native.api import api, ffi
    return api, ffi
