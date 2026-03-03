# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Tests for ChatClient – mirrors chatClient.test.ts."""

from __future__ import annotations

import pytest

from test.conftest import TEST_MODEL_ALIAS


def _get_loaded_chat_model(catalog):
    """Helper: ensure the test model is selected, loaded, and return Model + ChatClient."""
    cached = catalog.get_cached_models()
    assert len(cached) > 0

    cached_variant = next((m for m in cached if m.alias == TEST_MODEL_ALIAS), None)
    assert cached_variant is not None, f"{TEST_MODEL_ALIAS} should be cached"

    model = catalog.get_model(TEST_MODEL_ALIAS)
    assert model is not None

    model.select_variant(cached_variant)
    model.load()
    return model


class TestChatClient:
    """Chat Client Tests."""

    def test_should_perform_chat_completion(self, catalog):
        """Non-streaming chat: 7 * 6 should include '42' in the response."""
        model = _get_loaded_chat_model(catalog)
        try:
            client = model.get_chat_client()
            client.settings.max_completion_tokens = 500
            client.settings.temperature = 0.0  # deterministic

            result = client.complete_chat([
                {"role": "user",
                 "content": "You are a calculator. Be precise. What is the answer to 7 multiplied by 6?"}
            ])

            assert result is not None
            assert result.choices is not None
            assert len(result.choices) > 0
            assert result.choices[0].message is not None
            content = result.choices[0].message.content
            assert isinstance(content, str)
            assert "42" in content
        finally:
            model.unload()

    def test_should_perform_streaming_chat_completion(self, catalog):
        """Streaming chat: 7 * 6 = 42, then follow-up +25 = 67."""
        model = _get_loaded_chat_model(catalog)
        try:
            client = model.get_chat_client()
            client.settings.max_completion_tokens = 500
            client.settings.temperature = 0.0

            messages = [
                {"role": "user",
                 "content": "You are a calculator. Be precise. What is the answer to 7 multiplied by 6?"}
            ]

            # ---- First question ----
            full_content = []
            chunk_count = [0]

            def on_chunk_1(chunk):
                chunk_count[0] += 1
                delta = getattr(chunk.choices[0], "delta", None) if chunk.choices else None
                if delta and delta.content:
                    full_content.append(delta.content)

            client.complete_streaming_chat(messages, on_chunk_1)

            assert chunk_count[0] > 0
            first_response = "".join(full_content)
            assert isinstance(first_response, str)
            assert "42" in first_response

            # ---- Follow-up question ----
            messages.append({"role": "assistant", "content": first_response})
            messages.append({"role": "user", "content": "Add 25 to the previous answer. Think hard to be sure of the answer."})

            full_content.clear()
            chunk_count[0] = 0

            def on_chunk_2(chunk):
                chunk_count[0] += 1
                delta = getattr(chunk.choices[0], "delta", None) if chunk.choices else None
                if delta and delta.content:
                    full_content.append(delta.content)

            client.complete_streaming_chat(messages, on_chunk_2)

            assert chunk_count[0] > 0
            second_response = "".join(full_content)
            assert isinstance(second_response, str)
            assert "67" in second_response
        finally:
            model.unload()

    def test_should_raise_for_empty_messages(self, catalog):
        """complete_chat with empty list should raise."""
        model = catalog.get_model(TEST_MODEL_ALIAS)
        assert model is not None
        client = model.get_chat_client()

        with pytest.raises(Exception):
            client.complete_chat([])

    def test_should_raise_for_none_messages(self, catalog):
        """complete_chat with None should raise."""
        model = catalog.get_model(TEST_MODEL_ALIAS)
        assert model is not None
        client = model.get_chat_client()

        with pytest.raises(Exception):
            client.complete_chat(None)

    def test_should_raise_for_streaming_empty_messages(self, catalog):
        """complete_streaming_chat with empty list should raise."""
        model = catalog.get_model(TEST_MODEL_ALIAS)
        assert model is not None
        client = model.get_chat_client()

        with pytest.raises(Exception):
            client.complete_streaming_chat([], lambda chunk: None)

    def test_should_raise_for_streaming_none_messages(self, catalog):
        """complete_streaming_chat with None should raise."""
        model = catalog.get_model(TEST_MODEL_ALIAS)
        assert model is not None
        client = model.get_chat_client()

        with pytest.raises(Exception):
            client.complete_streaming_chat(None, lambda chunk: None)

    def test_should_raise_for_streaming_invalid_callback(self, catalog):
        """complete_streaming_chat with invalid callback should raise."""
        model = catalog.get_model(TEST_MODEL_ALIAS)
        assert model is not None
        client = model.get_chat_client()
        messages = [{"role": "user", "content": "Hello"}]

        for invalid_callback in [None, 42, {}, "not a function"]:
            with pytest.raises((TypeError, Exception)):
                client.complete_streaming_chat(messages, invalid_callback)
