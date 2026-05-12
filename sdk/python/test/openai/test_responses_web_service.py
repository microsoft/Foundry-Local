# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Integration tests for /v1/responses through the local web service.

These tests intentionally use FoundryLocalManager only for SDK setup, model
lifecycle, and web-service lifecycle. Actual Responses API calls go through the
OpenAI-compatible HTTP endpoint directly.
"""

from __future__ import annotations

import json

import pytest
import requests

from ..conftest import TEST_MODEL_ALIAS, skip_in_ci


pytestmark = skip_in_ci


def _response_text(response: dict) -> str:
    text = response.get("output_text")
    if isinstance(text, str) and text:
        return text

    return "".join(
        part.get("text", "")
        for item in response.get("output", []) or []
        if item.get("type") == "message"
        for part in item.get("content", []) or []
        if part.get("type") == "output_text" and isinstance(part.get("text"), str)
    )


def _post_response(base_url: str, body: dict) -> dict:
    response = requests.post(
        f"{base_url}/v1/responses",
        headers={"Content-Type": "application/json"},
        json=body,
        timeout=60,
    )
    assert response.ok, response.text
    return response.json()


def _post_streaming_response(base_url: str, body: dict) -> list[dict]:
    response = requests.post(
        f"{base_url}/v1/responses",
        headers={"Content-Type": "application/json", "Accept": "text/event-stream"},
        json={**body, "stream": True},
        stream=True,
        timeout=(60, None),
    )
    assert response.ok, response.text

    events: list[dict] = []
    buffer = ""
    try:
        for chunk in response.iter_content(chunk_size=None, decode_unicode=False):
            if not chunk:
                continue
            text = chunk.decode("utf-8", errors="replace") if isinstance(chunk, bytes) else chunk
            buffer += text.replace("\r\n", "\n")

            while "\n\n" in buffer:
                block, buffer = buffer.split("\n\n", 1)
                data = _sse_data(block)
                if not data:
                    continue
                if data == "[DONE]":
                    return events
                events.append(json.loads(data))
    finally:
        response.close()

    tail = buffer.strip()
    if tail:
        data = _sse_data(tail)
        if data and data != "[DONE]":
            events.append(json.loads(data))
    return events


def _sse_data(block: str) -> str:
    lines: list[str] = []
    for line in block.strip().split("\n"):
        if line.startswith("data: "):
            lines.append(line[6:])
        elif line == "data:":
            lines.append("")
    return "\n".join(lines).strip()


def _get_function_call(response: dict) -> dict | None:
    for item in response.get("output", []) or []:
        if item.get("type") == "function_call":
            return item
    return None


def _get_weather_tool() -> dict:
    return {
        "type": "function",
        "name": "get_weather",
        "description": "Get the current weather. This test always returns Seattle weather.",
        "parameters": {
            "type": "object",
            "properties": {},
            "additionalProperties": False,
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

        yield manager.urls[0].rstrip("/"), model.id
    finally:
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
        base_url, model_id = responses_web_service

        response = _post_response(
            base_url,
            {
                "model": model_id,
                "input": "What is 2 + 2? Answer with just the number.",
                "temperature": 0,
                "max_output_tokens": 64,
                "store": False,
            },
        )

        assert response["object"] == "response"
        assert response["status"] == "completed"
        assert _response_text(response).strip()

    def test_should_stream_response_events(self, responses_web_service):
        base_url, model_id = responses_web_service

        events = _post_streaming_response(
            base_url,
            {
                "model": model_id,
                "input": "Count from 1 to 3.",
                "temperature": 0,
                "max_output_tokens": 64,
                "store": False,
            },
        )

        assert any(event.get("type") == "response.created" for event in events)
        assert any(event.get("type") == "response.output_text.delta" for event in events)
        assert any(event.get("type") == "response.completed" for event in events)

    def test_should_round_trip_function_call_output(self, responses_web_service):
        base_url, model_id = responses_web_service
        weather_tool = _get_weather_tool()

        tool_response = _post_response(
            base_url,
            {
                "model": model_id,
                "input": "Use the get_weather tool and then answer with the weather.",
                "tools": [weather_tool],
                "tool_choice": "required",
                "temperature": 0,
                "max_output_tokens": 64,
                "store": True,
            },
        )
        function_call = _get_function_call(tool_response)

        assert function_call is not None, json.dumps(tool_response.get("output", []))
        assert function_call["name"] == "get_weather"
        assert isinstance(function_call["call_id"], str)

        final_response = _post_response(
            base_url,
            {
                "model": model_id,
                "previous_response_id": tool_response["id"],
                "input": [
                    {
                        "type": "function_call_output",
                        "call_id": function_call["call_id"],
                        "output": json.dumps({"location": "Seattle", "weather": "72 degrees F and sunny"}),
                    }
                ],
                "tools": [weather_tool],
                "temperature": 0,
                "max_output_tokens": 64,
                "store": False,
            },
        )

        assert final_response["status"] == "completed"
        assert _response_text(final_response).strip()
