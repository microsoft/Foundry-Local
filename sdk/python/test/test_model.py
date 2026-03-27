# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Tests for Model – mirrors model.test.ts."""

from __future__ import annotations

from .conftest import TEST_MODEL_ALIAS, AUDIO_MODEL_ALIAS


class TestModel:
    """Model Tests."""

    def test_should_verify_cached_models(self, catalog):
        """Cached models from test-data-shared should include qwen and whisper."""
        cached = catalog.get_cached_models()
        assert isinstance(cached, list)
        assert len(cached) > 0

        # Check qwen model is cached
        qwen = next((m for m in cached if m.alias == TEST_MODEL_ALIAS), None)
        assert qwen is not None, f"{TEST_MODEL_ALIAS} should be cached"
        assert qwen.is_cached is True

        # Check whisper model is cached
        whisper = next((m for m in cached if m.alias == AUDIO_MODEL_ALIAS), None)
        assert whisper is not None, f"{AUDIO_MODEL_ALIAS} should be cached"
        assert whisper.is_cached is True

    def test_should_load_and_unload_model(self, catalog):
        """Load/unload cycle should toggle is_loaded on the selected variant."""
        cached = catalog.get_cached_models()
        assert len(cached) > 0

        cached_variant = next((m for m in cached if m.alias == TEST_MODEL_ALIAS), None)
        assert cached_variant is not None

        model = catalog.get_model(TEST_MODEL_ALIAS)
        assert model is not None

        model.select_variant(cached_variant)

        # Ensure it's not loaded initially (or unload if it is)
        if model.is_loaded:
            model.unload()
        assert model.is_loaded is False

        try:
            model.load()
            assert model.is_loaded is True

            model.unload()
            assert model.is_loaded is False
        finally:
            # Safety cleanup
            if model.is_loaded:
                model.unload()
