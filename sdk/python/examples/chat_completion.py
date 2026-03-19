#!/usr/bin/env python3
# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------

"""Example: Chat completion using Foundry Local Python SDK.

Demonstrates basic chat completion with the Foundry Local runtime,
including model discovery, loading, and inference.
"""

from foundry_local_sdk import Configuration, FoundryLocalManager

def main():
    # 1. Initialize the SDK
    config = Configuration(app_name="ChatCompletionExample")
    print("Initializing Foundry Local Manager")
    FoundryLocalManager.initialize(config)
    manager = FoundryLocalManager.instance

    # 2. Print available models in the catalog and cache
    models = manager.catalog.list_models()
    print("Available models in catalog:")
    for m in models:
        print(f"  - {m.alias} ({m.id})")

    cached_models = manager.catalog.get_cached_models()
    print("\nCached models:")
    for m in cached_models:
        print(f"  - {m.alias} ({m.id})")

    CACHED_MODEL_ALIAS = "qwen2.5-0.5b"

    # 3. Find a model from the cache (+ download if not cached)
    model = manager.catalog.get_model(CACHED_MODEL_ALIAS)
    if model is None:
        print(f"Model '{CACHED_MODEL_ALIAS}' not found in catalog.")
        print("Available models:")
        for m in manager.catalog.list_models():
            print(f"  - {m.alias} ({m.id})")
        return

    if not model.is_cached:
        print(f"Downloading {model.alias}...")
        model.download(progress_callback=lambda pct: print(f"  {pct:.1f}%", end="\r"))
        print()

    # 4. Load the model
    print(f"Loading {model.alias}...", end="")
    model.load()
    print("loaded!")

    try:
        # 5. Create a chat client and send a message
        client = model.get_chat_client()

        print("\n--- Non-streaming ---")
        response = client.complete_chat(
            messages=[{"role": "user", "content": "What is the capital of France? Reply briefly."}]
        )
        print(f"Response: {response.choices[0].message.content}")

        # 6. Streaming example
        print("\n--- Streaming ---")

        def on_chunk(chunk):
            if chunk.choices and chunk.choices[0].delta and chunk.choices[0].delta.content:
                print(chunk.choices[0].delta.content, end="", flush=True)

        client.complete_streaming_chat(
            [{"role": "user", "content": "Tell me a short joke."}],
            on_chunk,
        )
        print()  # newline after streaming

    except Exception as e:
        print(f"Error during inference: {e}")

    finally:
        # 7. Cleanup
        model.unload()
        print("\nModel unloaded.")


if __name__ == "__main__":
    main()
