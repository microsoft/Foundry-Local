# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Integration tests for /v1/responses through the local web service.

These tests use FoundryLocalManager for SDK setup, model lifecycle, and web-service
lifecycle.  Actual Responses API calls go through ResponsesClient, which is backed
by the native openai SDK pointed at the local web service.
"""

from __future__ import annotations

import json

import pytest

from foundry_local_sdk import FoundryLocalManager
from foundry_local_sdk.openai import ResponsesClient

from ..conftest import TEST_MODEL_ALIAS, skip_in_ci


pytestmark = skip_in_ci

VISION_MODEL_ALIAS = "qwen3-vl-2b-instruct"
VISION_IMAGE_URL = (
    "https://raw.githubusercontent.com/microsoft/fluentui-emoji/main/assets/Camera/3D/camera_3d.png"
)


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


def _get_cached_model(catalog, model_alias: str):
    cached = catalog.get_cached_models()
    cached_variant = next((m for m in cached if m.alias == model_alias), None)
    if cached_variant is None:
        pytest.skip(f"{model_alias} must be cached to run Responses web-service tests")

    model = catalog.get_model(model_alias)
    if model is None:
        pytest.skip(f"{model_alias} was not found in the catalog")

    model.select_variant(cached_variant)
    return model


def _run_responses_web_service(manager: FoundryLocalManager, model):
    service_started = False
    model_loaded = False
    client: ResponsesClient | None = None

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

        client = manager.create_responses_client(model.id)
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


@pytest.fixture(scope="class")
def responses_web_service(manager, catalog):
    model = _get_cached_model(catalog, TEST_MODEL_ALIAS)
    yield from _run_responses_web_service(manager, model)


@pytest.fixture(scope="class")
def responses_vision_web_service(manager, catalog):
    model = _get_cached_model(catalog, VISION_MODEL_ALIAS)
    input_modalities = model.input_modalities or ""
    if "image" not in input_modalities.split(","):
        pytest.skip(f"{VISION_MODEL_ALIAS} does not advertise image input support")

    yield from _run_responses_web_service(manager, model)


class TestResponsesWebService:
    def test_should_create_non_streaming_response(self, responses_web_service):
        client, model_id = responses_web_service

        response = client.create(
            "What is 2 + 2? Answer with just the number.",
            temperature=0,
            max_output_tokens=64,
            store=False,
        )

        assert response.status == "completed", response.error
        assert response.output_text.strip(), "Expected non-empty assistant text"

    def test_should_stream_response_events(self, responses_web_service):
        client, model_id = responses_web_service

        event_types: list[str] = []
        client.create_streaming(
            "Count from 1 to 3.",
            callback=lambda e: event_types.append(getattr(e, "type", "")),
            temperature=0,
            max_output_tokens=64,
            store=False,
        )

        assert "response.created" in event_types, f"Events seen: {event_types}"
        assert "response.output_text.delta" in event_types, f"Events seen: {event_types}"
        assert "response.completed" in event_types, f"Events seen: {event_types}"

    def test_should_round_trip_function_call_output(self, responses_web_service):
        client, model_id = responses_web_service
        weather_tool = _get_weather_tool()

        tool_response = client.create(
            "Use the get_weather tool and then answer with the weather.",
            tools=[weather_tool],
            tool_choice="required",
            temperature=0,
            max_output_tokens=64,
            store=True,
        )

        function_call = next(
            (item for item in tool_response.output if item.type == "function_call"),
            None,
        )
        assert function_call is not None, (
            f"Expected a function_call item. Output: {tool_response.output}"
        )
        assert function_call.name == "get_weather"

        final_response = client.create(
            [
                {
                    "type": "function_call_output",
                    "call_id": function_call.call_id,
                    "output": json.dumps({"location": "Seattle", "weather": "72 degrees F and sunny"}),
                }
            ],
            previous_response_id=tool_response.id,
            tools=[weather_tool],
            temperature=0,
            max_output_tokens=64,
            store=False,
        )

        assert final_response.status == "completed"
        assert final_response.output_text.strip(), "Expected non-empty final assistant text"


class TestResponsesVisionWebService:
    def test_should_create_response_with_image_url(self, responses_vision_web_service):
        client, model_id = responses_vision_web_service

        response = client.create(
            [
                {
                    "type": "message",
                    "role": "user",
                    "content": [
                        {
                            "type": "input_text",
                            "text": "Describe this image in one short sentence.",
                        },
                        {
                            "type": "input_image",
                            "image_url": VISION_IMAGE_URL,
                            "media_type": "image/png",
                            "detail": "low",
                        },
                    ],
                }
            ],
            temperature=0,
            max_output_tokens=128,
            store=False,
        )

        assert response.status == "completed", response.error
        assert response.output_text.strip(), "Expected non-empty vision response text"
