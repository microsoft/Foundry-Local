# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Integration tests for /v1/responses through the local web service.

These tests intentionally use FoundryLocalManager only for SDK setup, model
lifecycle, and web-service lifecycle. Actual Responses API calls go through the
official OpenAI Python client against the local OpenAI-compatible endpoint.
"""

from __future__ import annotations

import json
from typing import Any

import pytest
from openai import OpenAI

from ..conftest import TEST_MODEL_ALIAS, skip_in_ci


pytestmark = skip_in_ci


def _field(value: Any, name: str, default: Any = None) -> Any:
    if isinstance(value, dict):
        return value.get(name, default)
    return getattr(value, name, default)


def _response_text(response: Any) -> str:
    text = _field(response, "output_text")
    if isinstance(text, str) and text:
        return text

    output_text = ""
    for item in _field(response, "output", []) or []:
        if _field(item, "type") != "message":
            continue
        for part in _field(item, "content", []) or []:
            if _field(part, "type") == "output_text":
                part_text = _field(part, "text", "")
                if isinstance(part_text, str):
                    output_text += part_text
    return output_text


def _get_function_call(response: Any) -> Any:
    for item in _field(response, "output", []) or []:
        if _field(item, "type") == "function_call":
            return item
    return None


def _get_weather_tool() -> dict[str, Any]:
    return {
        "type": "function",
        "name": "get_weather",
        "description": "Get the current weather for a city.",
        "parameters": {
            "type": "object",
            "properties": {
                "location": {
                    "type": "string",
                    "description": "The city and region, for example Seattle, WA.",
                }
            },
            "required": ["location"],
        },
    }


@pytest.fixture(scope="module")
def responses_web_service(manager, catalog):
    cached = catalog.get_cached_models()
    cached_variant = next((m for m in cached if m.alias == TEST_MODEL_ALIAS), None)
    if cached_variant is None:
        pytest.skip(f"{TEST_MODEL_ALIAS} must be cached to run Responses web-service tests")

    model = catalog.get_model(TEST_MODEL_ALIAS)
    if model is None:
        pytest.skip(f"{TEST_MODEL_ALIAS} was not found in the catalog")

    model.select_variant(cached_variant)
    client = None
    service_started = False
    model_loaded = False

    try:
        try:
            model.load()
            model_loaded = True
            manager.start_web_service()
            service_started = True
        except Exception as exc:
            message = str(exc)
            if "execute_command_with_binary" in message:
                pytest.skip(
                    "Local Foundry Local Core/native runtime is stale: "
                    "failed to resolve execute_command_with_binary"
                )
            pytest.skip(f"Failed to start Responses web-service test prerequisites: {exc}")

        if not manager.urls:
            pytest.skip("Web service started but did not return any URLs")

        base_url = manager.urls[0].rstrip("/") + "/v1"
        client = OpenAI(base_url=base_url, api_key="notneeded")
        if not hasattr(client, "responses"):
            pytest.skip("Installed openai package does not expose the Responses API")
        yield client, model.id
    finally:
        if client is not None:
            client.close()
        if service_started:
            try:
                manager.stop_web_service()
            except Exception:
                pass
        if model_loaded:
            try:
                model.unload()
            except Exception:
                pass


class TestResponsesWebService:
    def test_should_create_non_streaming_response(self, responses_web_service):
        client, model_id = responses_web_service

        response = client.responses.create(
            model=model_id,
            input="What is 2 + 2? Reply briefly.",
        )

        assert _response_text(response).strip()

    def test_should_stream_response_events(self, responses_web_service):
        client, model_id = responses_web_service
        saw_text_delta = False
        saw_completion = False

        stream = client.responses.create(
            model=model_id,
            input="Count from 1 to 3, separated by spaces.",
            stream=True,
        )
        for event in stream:
            event_type = _field(event, "type")
            if event_type == "response.output_text.delta" and _field(event, "delta"):
                saw_text_delta = True
            if event_type == "response.completed":
                saw_completion = True

        assert saw_text_delta
        assert saw_completion

    def test_should_round_trip_function_call_output(self, responses_web_service):
        client, model_id = responses_web_service
        weather_tool = _get_weather_tool()

        tool_response = client.responses.create(
            model=model_id,
            input="Use get_weather to check the weather in Seattle, then answer.",
            tools=[weather_tool],
            tool_choice="required",
            store=True,
        )
        function_call = _get_function_call(tool_response)

        assert function_call is not None
        assert _field(function_call, "name") == "get_weather"
        assert _field(function_call, "call_id")

        final_response = client.responses.create(
            model=model_id,
            previous_response_id=_field(tool_response, "id"),
            input=[
                {
                    "type": "function_call_output",
                    "call_id": _field(function_call, "call_id"),
                    "output": json.dumps(
                        {
                            "location": "Seattle, WA",
                            "temperature": "68 F",
                            "conditions": "sunny",
                        }
                    ),
                }
            ],
        )

        assert _response_text(final_response).strip()
