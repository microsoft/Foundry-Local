#!/usr/bin/env python3
# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------

"""Example: Responses API through the Foundry Local web service.

Foundry Local manages setup, model lifecycle, and the local OpenAI-compatible
web service. The official OpenAI Python client sends the actual /v1/responses
requests to that local service.
"""

from __future__ import annotations

import json
from typing import Any

from openai import OpenAI

from foundry_local_sdk import Configuration, FoundryLocalManager


MODEL_ALIAS = "qwen2.5-0.5b"


def _field(value: Any, name: str, default: Any = None) -> Any:
    if isinstance(value, dict):
        return value.get(name, default)
    return getattr(value, name, default)


def _response_text(response: Any) -> str:
    text = _field(response, "output_text")
    if isinstance(text, str) and text:
        return text

    for item in _field(response, "output", []) or []:
        if _field(item, "type") != "message":
            continue
        for part in _field(item, "content", []) or []:
            if _field(part, "type") == "output_text":
                part_text = _field(part, "text", "")
                if isinstance(part_text, str):
                    text = (text or "") + part_text
    return text or ""


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


def main() -> None:
    config = Configuration(app_name="ResponsesWebServiceExample")
    print("Initializing Foundry Local Manager")
    FoundryLocalManager.initialize(config)
    manager = FoundryLocalManager.instance
    if manager is None:
        raise RuntimeError("FoundryLocalManager.initialize did not set instance")

    print("Registering execution providers...")
    ep_result = manager.download_and_register_eps()
    print(f"EP registration success: {ep_result.success} ({ep_result.status})")

    model = manager.catalog.get_model(MODEL_ALIAS)
    if model is None:
        raise RuntimeError(f"Model '{MODEL_ALIAS}' not found in catalog")

    if not model.is_cached:
        print(f"Downloading {model.alias}...")
        model.download(progress_callback=lambda pct: print(f"  {pct:.1f}%", end="\r"))
        print()

    print(f"Loading {model.alias}...", end="")
    model.load()
    print("loaded!")

    openai_client: OpenAI | None = None
    try:
        print("Starting OpenAI-compatible web service...", end="")
        manager.start_web_service()
        if not manager.urls:
            raise RuntimeError("Web service started but did not return any URLs")
        print("started!")

        base_url = manager.urls[0].rstrip("/") + "/v1"
        openai_client = OpenAI(base_url=base_url, api_key="notneeded")

        print("\n--- Non-streaming Responses call ---")
        response = openai_client.responses.create(
            model=model.id,
            input="What is 2 + 2? Reply briefly.",
        )
        print(_response_text(response))

        print("\n--- Streaming Responses call ---")
        stream = openai_client.responses.create(
            model=model.id,
            input="Count from 1 to 3, separated by spaces.",
            stream=True,
        )
        for event in stream:
            if _field(event, "type") == "response.output_text.delta":
                print(_field(event, "delta", ""), end="", flush=True)
        print()

        print("\n--- Function/tool calling Responses flow ---")
        weather_tool = _get_weather_tool()
        tool_response = openai_client.responses.create(
            model=model.id,
            input="Use get_weather to check the weather in Seattle, then answer.",
            tools=[weather_tool],
            tool_choice="required",
            store=True,
        )
        function_call = _get_function_call(tool_response)
        if function_call is None:
            raise RuntimeError("Model did not return a function_call item")

        print(f"Tool call: {_field(function_call, 'name')}")
        print(f"Arguments: {_field(function_call, 'arguments')}")

        final_response = openai_client.responses.create(
            model=model.id,
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
        print(_response_text(final_response))

    finally:
        if openai_client is not None:
            openai_client.close()
        try:
            manager.stop_web_service()
            print("Web service stopped.")
        except Exception:
            pass
        model.unload()
        print("Model unloaded.")


if __name__ == "__main__":
    main()
