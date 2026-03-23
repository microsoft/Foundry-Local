# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Tests for ChatClient – mirrors chatClient.test.ts."""

from __future__ import annotations

import json

import pytest

from ..conftest import TEST_MODEL_ALIAS, get_multiply_tool


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
            client.settings.max_tokens = 500
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
            client.settings.max_tokens = 500
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

        with pytest.raises(ValueError):
            client.complete_chat([])

    def test_should_raise_for_none_messages(self, catalog):
        """complete_chat with None should raise."""
        model = catalog.get_model(TEST_MODEL_ALIAS)
        assert model is not None
        client = model.get_chat_client()

        with pytest.raises(ValueError):
            client.complete_chat(None)

    def test_should_raise_for_streaming_empty_messages(self, catalog):
        """complete_streaming_chat with empty list should raise."""
        model = catalog.get_model(TEST_MODEL_ALIAS)
        assert model is not None
        client = model.get_chat_client()

        with pytest.raises(ValueError):
            client.complete_streaming_chat([], lambda chunk: None)

    def test_should_raise_for_streaming_none_messages(self, catalog):
        """complete_streaming_chat with None should raise."""
        model = catalog.get_model(TEST_MODEL_ALIAS)
        assert model is not None
        client = model.get_chat_client()

        with pytest.raises(ValueError):
            client.complete_streaming_chat(None, lambda chunk: None)

    def test_should_raise_for_streaming_invalid_callback(self, catalog):
        """complete_streaming_chat raises TypeError only when an explicit non-callable
        is passed as the callback (third positional arg or keyword)."""
        model = catalog.get_model(TEST_MODEL_ALIAS)
        assert model is not None
        client = model.get_chat_client()
        messages = [{"role": "user", "content": "Hello"}]
        tools = [{"type": "function", "function": {"name": "f", "description": "d"}}]

        for invalid_callback in [42, {}, "not a function"]:
            with pytest.raises(TypeError):
                client.complete_streaming_chat(messages, tools, invalid_callback)

    def test_should_perform_streaming_chat_completion_with_iterator(self, catalog):
        """Iterator mode: complete_streaming_chat without callback yields chunks."""
        model = _get_loaded_chat_model(catalog)
        try:
            client = model.get_chat_client()
            client.settings.max_tokens = 500
            client.settings.temperature = 0.0

            chunks = list(client.complete_streaming_chat([
                {"role": "user",
                 "content": "You are a calculator. Be precise. What is the answer to 7 multiplied by 6?"}
            ]))

            assert len(chunks) > 0
            content = "".join(
                chunk.choices[0].delta.content
                for chunk in chunks
                if chunk.choices and chunk.choices[0].delta and chunk.choices[0].delta.content
            )
            assert "42" in content
        finally:
            model.unload()

    def test_should_perform_tool_calling_chat_completion(self, catalog):
        """Tool calling (non-streaming): model uses multiply_numbers tool to answer 7 * 6."""
        model = _get_loaded_chat_model(catalog)
        try:
            client = model.get_chat_client()
            client.settings.max_tokens = 500
            client.settings.temperature = 0.0
            client.settings.tool_choice = {"type": "required"}

            messages = [
                {"role": "system", "content": "You are a helpful AI assistant. If necessary, you can use any provided tools to answer the question."},
                {"role": "user", "content": "What is the answer to 7 multiplied by 6?"},
            ]
            tools = [get_multiply_tool()]

            # First turn: model should respond with a tool call
            response = client.complete_chat(messages, tools)

            assert response is not None
            assert response.choices is not None
            assert len(response.choices) > 0
            assert response.choices[0].finish_reason == "tool_calls"
            assert response.choices[0].message is not None
            assert response.choices[0].message.tool_calls is not None
            assert len(response.choices[0].message.tool_calls) > 0

            tool_call = response.choices[0].message.tool_calls[0]
            assert tool_call.type == "function"
            assert tool_call.function.name == "multiply_numbers"

            args = json.loads(tool_call.function.arguments)
            assert args["first"] == 7
            assert args["second"] == 6

            # Second turn: provide tool result and ask model to continue
            messages.append({"role": "tool", "content": "7 x 6 = 42."})
            messages.append({"role": "system", "content": "Respond only with the answer generated by the tool."})

            client.settings.tool_choice = {"type": "auto"}
            response = client.complete_chat(messages, tools)

            assert response.choices[0].message.content is not None
            assert "42" in response.choices[0].message.content
        finally:
            model.unload()

    def test_should_perform_tool_calling_streaming_chat_completion(self, catalog):
        """Tool calling (streaming): model uses multiply_numbers tool, then continue conversation."""
        model = _get_loaded_chat_model(catalog)
        try:
            client = model.get_chat_client()
            client.settings.max_tokens = 500
            client.settings.temperature = 0.0
            client.settings.tool_choice = {"type": "required"}

            messages = [
                {"role": "system", "content": "You are a helpful AI assistant. If necessary, you can use any provided tools to answer the question."},
                {"role": "user", "content": "What is the answer to 7 multiplied by 6?"},
            ]
            tools = [get_multiply_tool()]

            full_response = []
            last_tool_call_chunk = [None]

            def on_chunk_1(chunk):
                if not chunk.choices:
                    return
                delta = chunk.choices[0].delta
                if delta and delta.content:
                    full_response.append(delta.content)
                if delta and delta.tool_calls:
                    last_tool_call_chunk[0] = chunk

            client.complete_streaming_chat(messages, tools, on_chunk_1)

            assert last_tool_call_chunk[0] is not None

            tool_call_choice = last_tool_call_chunk[0].choices[0]
            assert tool_call_choice.finish_reason == "tool_calls"

            tool_call = tool_call_choice.delta.tool_calls[0]
            assert tool_call.type == "function"
            assert tool_call.function.name == "multiply_numbers"

            args = json.loads(tool_call.function.arguments)
            assert args["first"] == 7
            assert args["second"] == 6

            # Second turn: provide tool result and continue
            messages.append({"role": "tool", "content": "7 x 6 = 42."})
            messages.append({"role": "system", "content": "Respond only with the answer generated by the tool."})

            client.settings.tool_choice = {"type": "auto"}

            full_response.clear()

            def on_chunk_2(chunk):
                if chunk.choices:
                    delta = chunk.choices[0].delta
                    if delta and delta.content:
                        full_response.append(delta.content)

            client.complete_streaming_chat(messages, tools, on_chunk_2)

            second_response = "".join(full_response)
            assert isinstance(second_response, str)
            assert "42" in second_response
        finally:
            model.unload()

    def test_should_return_generator_when_no_callback_given(self, catalog):
        """Without a callback, complete_streaming_chat returns a generator."""
        model = _get_loaded_chat_model(catalog)
        try:
            client = model.get_chat_client()
            client.settings.max_tokens = 50
            client.settings.temperature = 0.0

            result = client.complete_streaming_chat([{"role": "user", "content": "Say hi."}])

            assert result is not None
            chunks = list(result)
            assert len(chunks) > 0
        finally:
            model.unload()

    def test_should_call_callback_and_return_none_when_callback_given(self, catalog):
        """With a callback, complete_streaming_chat calls it per chunk and returns None."""
        model = _get_loaded_chat_model(catalog)
        try:
            client = model.get_chat_client()
            client.settings.max_tokens = 50
            client.settings.temperature = 0.0

            received = []
            result = client.complete_streaming_chat(
                [{"role": "user", "content": "Say hi."}],
                lambda chunk: received.append(chunk),
            )

            assert result is None
            assert len(received) > 0
        finally:
            model.unload()