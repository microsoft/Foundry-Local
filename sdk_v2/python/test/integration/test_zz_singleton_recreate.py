# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Singleton lifecycle: close() clears the slot so a new manager can be built.

Named ``test_zz_...`` so it sorts after other integration tests; tearing
down and rebuilding the singleton mid-session would invalidate any cached
fixture references.

.. warning::
   **Fragile ordering invariant.** This file's ``restore_singleton`` fixture
   closes and rebuilds the global ``FoundryLocalManager`` singleton. The
   session-scoped ``manager`` fixture in ``conftest.py`` caches a reference
   to the *original* singleton, so any test that uses it **after** this file
   would get a stale reference.

   It works today because:

   1. pytest collects integration tests in alphabetical filename order, so
      ``test_zz_*.py`` runs after every other ``test_*.py``.
   2. The conftest teardown guards ``mgr.close()`` with
      ``FoundryLocalManager.instance is mgr`` so the post-rebuild reference
      mismatch is tolerated.

   If a future test file is added with a name that sorts after ``test_zz_``
   (or uses ``pytest-randomly`` / ``pytest-ordering``), this invariant breaks
   silently. The principled fix is subprocess isolation (``pytest-forked``
   with ``@pytest.mark.forked``) or a separate CI invocation for singleton-
   lifecycle tests. Defer until a third lifecycle test exists or the
   collection order changes.
"""
from __future__ import annotations

import pytest

from foundry_local_sdk import Configuration, FoundryLocalManager, LogLevel


def _make_config(manager) -> Configuration:
    """Mirror the conftest manager config so the rebuilt singleton uses the same cache."""
    src = manager.config
    kwargs: dict[str, object] = {
        "app_name": src.app_name,
        "log_level": src.log_level or LogLevel.WARNING,
    }
    if src.model_cache_dir:
        kwargs["model_cache_dir"] = src.model_cache_dir
    if src.app_data_dir:
        kwargs["app_data_dir"] = src.app_data_dir
    if src.logs_dir:
        kwargs["logs_dir"] = src.logs_dir
    return Configuration(**kwargs)


@pytest.fixture
async def restore_singleton(manager):
    """Save the current config, run the test, then leave a working singleton in place."""
    saved_config = _make_config(manager)
    yield saved_config
    if FoundryLocalManager.instance is None:
        await FoundryLocalManager.initialize(saved_config)


class TestSingletonRecreate:
    async def test_close_clears_singleton(self, restore_singleton):
        config = restore_singleton
        # Close whatever singleton currently exists.
        assert FoundryLocalManager.instance is not None
        await FoundryLocalManager.instance.close()
        assert FoundryLocalManager.instance is None

        # A fresh manager can now be constructed and registers as the singleton.
        await FoundryLocalManager.initialize(config)
        assert FoundryLocalManager.instance is not None

    async def test_close_is_idempotent(self, restore_singleton):
        mgr = FoundryLocalManager.instance
        assert mgr is not None
        await mgr.close()
        # Second close must not raise.
        await mgr.close()
        assert FoundryLocalManager.instance is None

    async def test_context_manager_clears_singleton(self, restore_singleton):
        config = restore_singleton
        # Tear down the existing singleton so we can build a new one.
        if FoundryLocalManager.instance is not None:
            await FoundryLocalManager.instance.close()

        await FoundryLocalManager.initialize(config)
        async with FoundryLocalManager.instance as m:
            assert FoundryLocalManager.instance is m
        assert FoundryLocalManager.instance is None
