# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Tests for Model – mirrors model.test.ts."""

from __future__ import annotations

import threading

from types import SimpleNamespace

from foundry_local_sdk.detail.model_variant import ModelVariant

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

    def test_should_expose_context_length(self, catalog):
        """Model should expose context_length from ModelInfo metadata."""
        model = catalog.get_model(TEST_MODEL_ALIAS)
        assert model is not None
        # context_length should be None or a positive integer
        ctx = model.context_length
        assert ctx is None or (isinstance(ctx, int) and ctx > 0)

    def test_should_expose_modalities(self, catalog):
        """Model should expose input_modalities and output_modalities."""
        model = catalog.get_model(TEST_MODEL_ALIAS)
        assert model is not None
        # Modalities should be None or non-empty strings
        for val in (model.input_modalities, model.output_modalities):
            assert val is None or (isinstance(val, str) and len(val) > 0)

    def test_should_expose_capabilities(self, catalog):
        """Model should expose capabilities metadata."""
        model = catalog.get_model(TEST_MODEL_ALIAS)
        assert model is not None
        caps = model.capabilities
        assert caps is None or (isinstance(caps, str) and len(caps) > 0)

    def test_should_expose_supports_tool_calling(self, catalog):
        """Model should expose supports_tool_calling metadata."""
        model = catalog.get_model(TEST_MODEL_ALIAS)
        assert model is not None
        stc = model.supports_tool_calling
        assert stc is None or isinstance(stc, bool)

    def test_download_should_use_callback_path_when_cancel_event_is_provided(self):
        """Model download should route through callback interop when cancellation is enabled."""

        class _Response:
            def __init__(self, data=None, error=None):
                self.data = data
                self.error = error

        class _FakeCoreInterop:
            def __init__(self):
                self.calls = []

            def execute_command(self, command_name, command_input=None):
                raise AssertionError(
                    "download should not use execute_command when cancel_event is provided"
                )

            def execute_command_with_callback(
                self, command_name, command_input=None, callback=None, cancel_event=None
            ):
                self.calls.append((command_name, command_input, callback, cancel_event))
                return _Response(data="", error=None)

        fake_core = _FakeCoreInterop()
        cancel_event = threading.Event()
        variant = ModelVariant.__new__(ModelVariant)
        variant._model_info = SimpleNamespace(id="test-model-cpu:1", alias=TEST_MODEL_ALIAS)
        variant._id = "test-model-cpu:1"
        variant._alias = TEST_MODEL_ALIAS
        variant._core_interop = fake_core
        variant._model_load_manager = None

        variant.download(cancel_event=cancel_event)

        assert len(fake_core.calls) == 1
        command_name, command_input, callback, seen_cancel_event = fake_core.calls[0]
        assert command_name == "download_model"
        assert command_input.params == {"Model": "test-model-cpu:1"}
        assert callable(callback)
        assert seen_cancel_event is cancel_event
