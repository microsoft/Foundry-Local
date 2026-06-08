#!/usr/bin/env python3
# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------

"""Example: Chat completion using the Foundry Local Python SDK (v2).

Demonstrates the typed ``ChatSession`` API: model discovery, optional EP
registration, download/load, non-streaming and streaming inference, and
cleanup.
"""

import os

from foundry_local_sdk import (
    ChatSession,
    Configuration,
    FoundryLocalManager,
    MessageItem,
    Request,
    RequestOptions,
    SearchOptions,
    TextItem,
)


CACHED_MODEL_ALIAS = "qwen2.5-0.5b"


def _print_response_text(response) -> None:
    """Print the text content of a chat response.

    Non-streaming chat responses yield ``MessageItem`` (the assistant turn)
    whose ``.parts`` hold the ``TextItem``\\ s; loose ``TextItem``\\ s may
    also appear directly. Handle both.
    """
    for item in response:
        if isinstance(item, MessageItem):
            for part in item.parts:
                if isinstance(part, TextItem):
                    print(part.text, end="")
        elif isinstance(item, TextItem):
            print(item.text, end="")
    print()


def main() -> None:
    # 1. Initialize the SDK
    config = Configuration(
        app_name="ChatCompletionExample",
    )
    print("Initializing Foundry Local Manager")
    FoundryLocalManager.initialize(config)
    manager = FoundryLocalManager.instance

    # Discover available EPs and register them so hardware-accelerated providers are usable.
    eps = manager.discover_eps()
    print("Available execution providers:")
    for ep in eps:
        print(f"  - {ep.name} (registered: {ep.is_registered})")

    ep_result = manager.download_and_register_eps()
    print(f"EP registration success: {ep_result.success} ({ep_result.status})")

    # 2. Print available models in the catalog and cache
    print("\nAvailable models in catalog:")
    for m in manager.catalog.list_models():
        print(f"  - {m.alias} ({m.id})")

    print("\nCached models:")
    for m in manager.catalog.get_cached_models():
        print(f"  - {m.alias} ({m.id})")

    # 3. Find a model (download if not cached)
    model = manager.catalog.get_model(CACHED_MODEL_ALIAS)
    if model is None:
        print(f"\nModel '{CACHED_MODEL_ALIAS}' not found in catalog.")
        return

    if not model.is_cached:
        print(f"\nDownloading {model.alias}...")
        model.download(progress_callback=lambda pct: print(f"  {pct:.1f}%", end="\r"))
        print()

    # 4. Load the model
    print(f"\nLoading {model.alias}...", end="", flush=True)
    model.load()
    print("loaded!")

    try:
        # 5. Non-streaming: a single ChatSession driving two turns.
        #
        # ChatSession is stateful — the session retains the conversation
        # history across calls to process_request, so the second turn only
        # needs to add the new user message. The model sees the prior
        # question and its own answer when responding.
        print("\n--- Non-streaming (multi-turn) ---")
        with ChatSession(model) as session:
            session.set_options(RequestOptions(search=SearchOptions(temperature=0, max_output_tokens=256)))

            # Turn 1: ask the initial question.
            with Request().add_item(
                MessageItem.user("What is the capital of France? Reply briefly.")
            ) as req:
                with session.process_request(req) as response:
                    print("User: What is the capital of France? Reply briefly.")
                    print("Assistant: ", end="", flush=True)
                    _print_response_text(response)

            # Turn 2: follow-up that only makes sense if the session
            # remembers turn 1. Only the new user message is added.
            with Request().add_item(
                MessageItem.user("And what country is it in?")
            ) as req:
                with session.process_request(req) as response:
                    print("User: And what country is it in?")
                    print("Assistant: ", end="", flush=True)
                    _print_response_text(response)

            print(f"(session turn_count = {session.turn_count})")

        # 6. Streaming: a fresh ChatSession for a single streamed turn.
        print("\n--- Streaming ---")
        with ChatSession(model) as session:
            session.set_options(RequestOptions(search=SearchOptions(temperature=0, max_output_tokens=256)))
            session.set_streaming(True)

            with Request().add_item(MessageItem.user("Tell me a short joke.")) as req:
                print("User: Tell me a short joke.")
                print("Assistant: ", end="", flush=True)
                for item in session.process_streaming_request(req):
                    if isinstance(item, TextItem):
                        print(item.text, end="", flush=True)
            print()  # newline after streaming
    finally:
        # 6. Cleanup
        model.unload()
        print("\nModel unloaded.")


if __name__ == "__main__":
    main()
