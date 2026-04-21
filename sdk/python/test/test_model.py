# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Tests for Model – mirrors model.test.ts."""

from __future__ import annotations

import pytest

from .conftest import TEST_MODEL_ALIAS, AUDIO_MODEL_ALIAS


class TestModel:
    """Model Tests."""

    @pytest.mark.asyncio
    async def test_should_verify_cached_models(self, catalog):
        """Cached models from test-data-shared should include qwen and whisper."""
        cached = await catalog.get_cached_models()
        assert isinstance(cached, list)
        assert len(cached) > 0

        # Check qwen model is cached
        qwen = next((m for m in cached if m.alias == TEST_MODEL_ALIAS), None)
        assert qwen is not None, f"{TEST_MODEL_ALIAS} should be cached"
        assert await qwen.is_cached() is True

        # Check whisper model is cached
        whisper = next((m for m in cached if m.alias == AUDIO_MODEL_ALIAS), None)
        assert whisper is not None, f"{AUDIO_MODEL_ALIAS} should be cached"
        assert await whisper.is_cached() is True

    @pytest.mark.asyncio
    async def test_should_load_and_unload_model(self, catalog):
        """Load/unload cycle should toggle is_loaded on the selected variant."""
        cached = await catalog.get_cached_models()
        assert len(cached) > 0

        cached_variant = next((m for m in cached if m.alias == TEST_MODEL_ALIAS), None)
        assert cached_variant is not None

        model = await catalog.get_model(TEST_MODEL_ALIAS)
        assert model is not None

        model.select_variant(cached_variant)

        # Ensure it's not loaded initially (or unload if it is)
        if await model.is_loaded():
            await model.unload()
        assert await model.is_loaded() is False

        try:
            await model.load()
            assert await model.is_loaded() is True

            await model.unload()
            assert await model.is_loaded() is False
        finally:
            # Safety cleanup
            if await model.is_loaded():
                await model.unload()

    @pytest.mark.asyncio
    async def test_should_expose_context_length(self, catalog):
        """Model should expose context_length from ModelInfo metadata."""
        model = await catalog.get_model(TEST_MODEL_ALIAS)
        assert model is not None
        # context_length should be None or a positive integer
        ctx = model.context_length
        assert ctx is None or (isinstance(ctx, int) and ctx > 0)

    @pytest.mark.asyncio
    async def test_should_expose_modalities(self, catalog):
        """Model should expose input_modalities and output_modalities."""
        model = await catalog.get_model(TEST_MODEL_ALIAS)
        assert model is not None
        # Modalities should be None or non-empty strings
        for val in (model.input_modalities, model.output_modalities):
            assert val is None or (isinstance(val, str) and len(val) > 0)

    @pytest.mark.asyncio
    async def test_should_expose_capabilities(self, catalog):
        """Model should expose capabilities metadata."""
        model = await catalog.get_model(TEST_MODEL_ALIAS)
        assert model is not None
        caps = model.capabilities
        assert caps is None or (isinstance(caps, str) and len(caps) > 0)

    @pytest.mark.asyncio
    async def test_should_expose_supports_tool_calling(self, catalog):
        """Model should expose supports_tool_calling metadata."""
        model = await catalog.get_model(TEST_MODEL_ALIAS)
        assert model is not None
        stc = model.supports_tool_calling
        assert stc is None or isinstance(stc, bool)
