# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Integration tests for the Responses API client.

These require a real Foundry Local runtime + a cached model. They are only
run when ``FOUNDRY_INTEGRATION_TESTS=1`` is set in the environment.
"""

from __future__ import annotations

import json
import os

import pytest

from foundry_local_sdk import (
    FunctionToolDefinition,
    InputImageContent,
    InputTextContent,
    MessageItem,
)

from ..conftest import TEST_MODEL_ALIAS

pytestmark = pytest.mark.skipif(
    not os.environ.get("FOUNDRY_INTEGRATION_TESTS"),
    reason="Set FOUNDRY_INTEGRATION_TESTS=1 to run Responses API integration tests.",
)


def _get_loaded_model(catalog):
    cached = catalog.get_cached_models()
    assert cached, "No cached models found"
    variant = next((m for m in cached if m.alias == TEST_MODEL_ALIAS), None)
    assert variant is not None, f"{TEST_MODEL_ALIAS} should be cached"

    model = catalog.get_model(TEST_MODEL_ALIAS)
    assert model is not None
    model.select_variant(variant)
    model.load()
    return model


@pytest.fixture(scope="module")
def responses_client(manager, catalog):
    """Start the web service, return a ResponsesClient tied to the test model."""
    model = _get_loaded_model(catalog)
    manager.start_web_service()
    client = manager.create_responses_client(model.id)
    try:
        yield client
    finally:
        try:
            manager.stop_web_service()
        finally:
            model.unload()


# ---------------------------------------------------------------------------
# Non-streaming
# ---------------------------------------------------------------------------

class TestNonStreaming:
    def test_simple_string_input(self, responses_client):
        resp = responses_client.create("What is 2 + 2? Reply with just the number.")
        assert resp.id
        assert resp.status in {"completed", "incomplete"}
        assert resp.output_text  # Non-empty

    def test_with_options(self, responses_client):
        resp = responses_client.create(
            "Say hello.",
            temperature=0.0,
            max_output_tokens=32,
        )
        assert resp.output_text

    def test_structured_input(self, responses_client):
        # Validates that structured MessageItem input is accepted and produces
        # a well-formed response. Not asserting content (too model-dependent).
        resp = responses_client.create(
            [
                MessageItem(role="user", content="Reply with the single word: ping"),
            ],
            temperature=0.0,
        )
        assert resp.status in {"completed", "incomplete"}
        assert resp.output_text.strip()

    def test_with_instructions(self, responses_client):
        resp = responses_client.create(
            "Who are you?",
            instructions="You are a terse assistant. Answer in exactly three words.",
            temperature=0.0,
        )
        assert resp.output_text

    def test_multi_turn(self, responses_client):
        # Validates previous_response_id wiring: the second response should
        # link back to the first via previous_response_id. We don't assert on
        # recall quality (too model-dependent for tiny test models).
        first = responses_client.create(
            "My favourite colour is green. Just acknowledge with 'ok'.",
            temperature=0.0,
            store=True,
        )
        assert first.id
        second = responses_client.create(
            "What colour did I mention?",
            previous_response_id=first.id,
            temperature=0.0,
        )
        assert second.previous_response_id == first.id
        assert second.output_text.strip()


# ---------------------------------------------------------------------------
# Streaming
# ---------------------------------------------------------------------------

class TestStreaming:
    def test_basic_streaming(self, responses_client):
        chunks = []
        completed = False
        for event in responses_client.create_streaming(
            "Count 1, 2, 3. Reply with just the digits separated by spaces.",
            temperature=0.0,
        ):
            if event.type == "response.output_text.delta":
                chunks.append(event.delta)
            elif event.type == "response.completed":
                completed = True
        assert completed
        assert "".join(chunks).strip()

    def test_streaming_with_options(self, responses_client):
        saw_completed = False
        for event in responses_client.create_streaming(
            "Hello",
            temperature=0.0,
            max_output_tokens=16,
        ):
            if event.type == "response.completed":
                saw_completed = True
        assert saw_completed

    def test_streaming_events_sequence(self, responses_client):
        # Expect created → in_progress → ... → completed
        types_seen = []
        for event in responses_client.create_streaming("Say hi.", temperature=0.0):
            types_seen.append(event.type)
        assert "response.created" in types_seen
        assert "response.completed" in types_seen
        assert types_seen.index("response.created") < types_seen.index("response.completed")


# ---------------------------------------------------------------------------
# Storage: get / delete / list
# ---------------------------------------------------------------------------

class TestStorage:
    def test_get_stored_response(self, responses_client):
        first = responses_client.create("Store this.", store=True, temperature=0.0)
        fetched = responses_client.get(first.id)
        assert fetched.id == first.id
        assert fetched.output_text == first.output_text

    def test_delete_response(self, responses_client):
        created = responses_client.create("Delete me.", store=True, temperature=0.0)
        result = responses_client.delete(created.id)
        assert result.id == created.id
        assert result.deleted is True

    def test_list_responses(self, responses_client):
        # Create one so the list is guaranteed non-empty.
        responses_client.create("A listable response.", store=True, temperature=0.0)
        result = responses_client.list()
        assert result.object == "list"
        assert len(result.data) >= 1


# ---------------------------------------------------------------------------
# Tool calling
# ---------------------------------------------------------------------------

class TestToolCalling:
    def test_function_call_round_trip(self, responses_client):
        tool = FunctionToolDefinition(
            name="multiply_numbers",
            description="Multiply two integers.",
            parameters={
                "type": "object",
                "properties": {
                    "a": {"type": "integer"},
                    "b": {"type": "integer"},
                },
                "required": ["a", "b"],
            },
        )
        first = responses_client.create(
            "What is 7 times 6? Use the multiply_numbers tool.",
            tools=[tool],
            temperature=0.0,
        )

        # Find the function_call item.
        call = next(
            (item for item in first.output if getattr(item, "type", None) == "function_call"),
            None,
        )
        if call is None:
            pytest.skip("Model did not emit a tool call for this prompt")

        args = json.loads(call.arguments)
        # Model may use the declared parameter names or invent its own.
        # Extract the two integer values robustly.
        int_values = [int(v) for v in args.values() if isinstance(v, (int, str)) and str(v).lstrip("-").isdigit()]
        if len(int_values) < 2:
            pytest.skip(f"Model produced unusable tool args: {args!r}")
        product = int_values[0] * int_values[1]

        follow = responses_client.create(
            [
                MessageItem(role="user", content="What is 7 times 6? Use the multiply_numbers tool."),
                call,
                {
                    "type": "function_call_output",
                    "call_id": call.call_id,
                    "output": str(product),
                },
            ],
            tools=[tool],
            temperature=0.0,
        )
        # Validates the round-trip: the follow-up should produce a completed
        # response that references the tool output. We don't assert content.
        assert follow.status in {"completed", "incomplete"}
        assert follow.output_text.strip()


# ---------------------------------------------------------------------------
# Vision
# ---------------------------------------------------------------------------

class TestVision:
    """These tests require a vision-capable model and will be skipped otherwise."""

    def _run_or_skip(self, responses_client, content):
        try:
            return responses_client.create(
                [MessageItem(role="user", content=content)],
                temperature=0.0,
            )
        except Exception as e:
            pytest.skip(f"Model does not appear to support vision: {e}")

    def test_image_base64_input(self, responses_client):
        # Minimal 1x1 PNG.
        png = bytes.fromhex(
            "89504e470d0a1a0a0000000d49484452000000010000000108060000001f15c4"
            "890000000d49444154789c6300010000000500010d0a2db40000000049454e44"
            "ae426082"
        )
        resp = self._run_or_skip(
            responses_client,
            [
                InputTextContent(text="Describe this image briefly."),
                InputImageContent.from_bytes(png, "image/png"),
            ],
        )
        assert resp.status in {"completed", "incomplete"}

    def test_image_with_text(self, responses_client):
        png = bytes.fromhex(
            "89504e470d0a1a0a0000000d49484452000000010000000108060000001f15c4"
            "890000000d49444154789c6300010000000500010d0a2db40000000049454e44"
            "ae426082"
        )
        resp = self._run_or_skip(
            responses_client,
            [
                InputTextContent(text="What colour is this?"),
                InputImageContent.from_bytes(png, "image/png"),
            ],
        )
        assert resp.status in {"completed", "incomplete"}
