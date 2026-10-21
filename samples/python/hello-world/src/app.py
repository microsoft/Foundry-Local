#!/usr/bin/env python3
"""End-to-end native Foundry Local chat sample."""

from __future__ import annotations

from foundry_local_sdk import (
    ChatSession,
    Configuration,
    FoundryLocalManager,
    MessageItem,
    Request,
    RequestOptions,
    SearchOptions,
    TextItem,
    TextItemType,
)

APP_NAME = "foundry_local_samples"
MODEL_ALIAS = "qwen2.5-0.5b"


def print_download_progress(percent: float) -> None:
    print(f"\rDownloading model: {percent:5.1f}%", end="", flush=True)


def main() -> None:
    # Manager owns catalogs and coordinates the lifetime of models and sessions.
    manager = FoundryLocalManager(Configuration(app_name=APP_NAME))
    model = None

    try:
        print("Discovering execution providers:")
        execution_providers = manager.discover_eps()
        for execution_provider in execution_providers:
            state = "registered" if execution_provider.is_registered else "available"
            print(f"  {execution_provider.name}: {state}")

        missing_eps = [
            execution_provider.name
            for execution_provider in execution_providers
            if not execution_provider.is_registered
        ]
        if missing_eps:
            print("Downloading and registering execution providers...")
            result = manager.download_and_register_eps(missing_eps)
            if not result.success:
                raise RuntimeError(
                    f"Execution provider registration failed: {result.failed_eps}"
                )

        catalog = manager.catalog
        models = catalog.list_models()
        print(f"\nCatalog '{catalog.name}' contains {len(models)} model aliases.")
        for available_model in models[:5]:
            print(f"  {available_model.alias} ({available_model.id})")

        model = catalog.get_model(MODEL_ALIAS)
        if model is None:
            raise RuntimeError(f"Model {MODEL_ALIAS!r} was not found in the catalog.")

        print(f"\nSelected {model.alias} ({model.id})")
        if model.is_cached:
            print("Model is already cached; skipping download.")
        else:
            model.download(progress_callback=print_download_progress)
            print()

        model.load()
        print("Model loaded.")

        with ChatSession(model) as session:
            session.set_options(
                RequestOptions(
                    search=SearchOptions(temperature=0.0, max_output_tokens=128)
                )
            )
            session.set_streaming(True)

            with Request().add_item(
                MessageItem.user("Explain in two sentences why the sky appears blue.")
            ) as request:
                print("\nAssistant: ", end="", flush=True)
                with session.process_streaming_request(request) as stream:
                    for item in stream:
                        if (
                            isinstance(item, TextItem)
                            and item.type is TextItemType.DEFAULT
                        ):
                            print(item.text, end="", flush=True)
                    with stream.final_response as response:
                        print(f"\nFinish reason: {response.finish_reason.name}")
    finally:
        if model is not None and model.is_loaded:
            model.unload()
            print("Model unloaded.")
        manager.close()
        print("Manager closed.")


if __name__ == "__main__":
    main()
