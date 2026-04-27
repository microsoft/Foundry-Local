# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""End-to-end example for the OpenAI Responses API client.

Run with::

    python examples/responses.py

Requires a loaded model and a started web service.
"""

from __future__ import annotations

import json

from foundry_local_sdk import (
    Configuration,
    FoundryLocalManager,
    FunctionToolDefinition,
    InputImageContent,
    InputTextContent,
    MessageItem,
)

MODEL_ALIAS = "phi-4-mini"


def setup():
    config = Configuration(app_name="ResponsesExample")
    FoundryLocalManager.initialize(config)
    mgr = FoundryLocalManager.instance

    mgr.download_and_register_eps()

    model = mgr.catalog.get_model(MODEL_ALIAS)
    if model is None:
        raise RuntimeError(f"Model '{MODEL_ALIAS}' not found in catalog")
    if not model.is_cached:
        print(f"Downloading {MODEL_ALIAS}...")
        model.download(progress_callback=lambda p: print(f"  {p:.1f}%", end="\r"))
        print()
    print(f"Loading {model.alias}...", end="")
    model.load()
    print("loaded!")
    mgr.start_web_service()

    client = mgr.create_responses_client(model.id)
    return mgr, model, client


def basic_create(client):
    print("\n=== 1. Basic create ===")
    resp = client.create("What is 2 + 2? Answer in one word.")
    print(f"status={resp.status}  text={resp.output_text!r}")


def streaming(client):
    print("\n=== 2. Streaming ===")
    print("assistant: ", end="", flush=True)
    for event in client.create_streaming("Count from 1 to 5, separated by spaces."):
        if event.type == "response.output_text.delta":
            print(event.delta, end="", flush=True)
        elif event.type == "response.completed":
            response = getattr(event, "response", None)
            usage = getattr(response, "usage", None) if response is not None else None
            total = getattr(usage, "total_tokens", None) if usage is not None else None
            print(f"\n(completed{f', {total} tokens' if total is not None else ''})")


def multi_turn(client):
    print("\n=== 3. Multi-turn ===")
    first = client.create("My favorite color is green. Remember that.", store=True)
    print(f"first id={first.id!r}")
    second = client.create(
        "What is my favorite color?",
        previous_response_id=first.id,
    )
    print(f"second: {second.output_text!r}")


def tool_calling(client):
    print("\n=== 4. Tool calling ===")
    tools = [
        FunctionToolDefinition(
            name="multiply_numbers",
            description="Multiply two integers together.",
            parameters={
                "type": "object",
                "properties": {
                    "a": {"type": "integer"},
                    "b": {"type": "integer"},
                },
                "required": ["a", "b"],
            },
        )
    ]
    resp = client.create("What is 7 times 6?", tools=tools)

    # Find a function_call item in the output (if the model produced one).
    for item in resp.output:
        if getattr(item, "type", None) == "function_call":
            print(f"call {item.name}({item.arguments})")
            args = json.loads(item.arguments)
            answer = args["a"] * args["b"]
            follow = client.create(
                [
                    MessageItem(role="user", content="What is 7 times 6?"),
                    item,
                    # The function_call_output is sent back keyed by call_id
                    {"type": "function_call_output", "call_id": item.call_id, "output": str(answer)},
                ],
                tools=tools,
            )
            print(f"final: {follow.output_text!r}")
            return
    print(f"no tool call — got text: {resp.output_text!r}")


def vision(client):
    print("\n=== 5. Vision ===")
    # Requires a vision-capable model. Replace with a real PNG to see real output.
    tiny_png = bytes.fromhex(
        "89504e470d0a1a0a0000000d49484452000000010000000108060000001f15c4"
        "890000000d49444154789c6300010000000500010d0a2db40000000049454e44"
        "ae426082"
    )
    msg = MessageItem(
        role="user",
        content=[
            InputTextContent(text="Describe this image in one sentence."),
            InputImageContent.from_bytes(tiny_png, "image/png"),
        ],
    )
    try:
        resp = client.create([msg])
        print(f"vision response: {resp.output_text!r}")
    except Exception as e:
        print(f"(skipped — model may not support vision: {e})")


def main():
    mgr, model, client = setup()
    try:
        basic_create(client)
        streaming(client)
        multi_turn(client)
        tool_calling(client)
        vision(client)
    finally:
        mgr.stop_web_service()
        model.unload()


if __name__ == "__main__":
    main()
