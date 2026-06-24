# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Model lifecycle (load / unload / download) integration tests.

WARNING — the ``@pytest.mark.manual`` test in this module deletes model files from disk (via
``IModel.remove_from_cache``) and re-downloads them. Run with ``pytest -m manual``. **Not safe to run in CI or on
a machine where you care about the model cache** — the download portion is bounded only by the network and the
model's actual size.
"""
from __future__ import annotations

import pytest


# ---------------------------------------------------------------------------
# Default (always-run) tests — load/unload on a cached model
# ---------------------------------------------------------------------------

class TestLoadUnload:
    async def test_load_idempotent(self, chat_model):
        # The fixture already loaded this model; load() again must be a no-op.
        await chat_model.load()
        assert chat_model.is_loaded is True

    async def test_unload_then_load(self, chat_model):
        try:
            await chat_model.unload()
            assert chat_model.is_loaded is False

            await chat_model.load()
            assert chat_model.is_loaded is True
        finally:
            # Leave the model loaded so other session-scoped consumers of ``chat_model`` see the same state
            # they expect.
            if not chat_model.is_loaded:
                await chat_model.load()

    def test_is_cached_true_for_fixture_model(self, chat_model):
        # The fixture selection requires is_cached — sanity check the invariant.
        assert chat_model.is_cached is True


# ---------------------------------------------------------------------------
# Manual test — download
# ---------------------------------------------------------------------------

@pytest.mark.manual
async def test_download_progress_callback_fires(chat_model):
    """Verify the download progress callback fires at least once.

    Removes the model from cache via the public ``remove_from_cache`` API (the SDK's supported way to force a
    re-download — preferable to a raw filesystem ``rmtree`` because it also clears any native bookkeeping).
    Then re-downloads with a progress callback and asserts it was invoked at least once with a value in
    ``[0.0, 100.0]``.

    The model is loaded again at the end so downstream tests see it in the same state as the session-scoped
    fixture would.
    """
    # Unload first — removing a loaded model from cache is not supported.
    if chat_model.is_loaded:
        await chat_model.unload()

    chat_model.remove_from_cache()
    assert chat_model.is_cached is False

    received: list[float] = []

    def on_progress(pct: float) -> None:
        received.append(pct)

    try:
        await chat_model.download(progress_callback=on_progress)
        assert chat_model.is_cached is True
        assert len(received) >= 1
        for pct in received:
            assert 0.0 <= pct <= 100.0
    finally:
        # Restore the loaded state so session-scoped fixture consumers are not disturbed.
        if chat_model.is_cached and not chat_model.is_loaded:
            await chat_model.load()
