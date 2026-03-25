#!/usr/bin/env python3

import sys
import argparse
from openai import OpenAI
from foundry_local import FoundryLocalManager


def read_file_content(file_path):
    """Read content from a file."""
    try:
        with open(file_path, "r", encoding="utf-8") as file:
            return file.read()
    except Exception as e:
        print(f"Error reading file: {e}")
        sys.exit(1)


def get_summary(text, client, model_name):
    """Get summary from OpenAI API."""
    try:
        response = client.chat.completions.create(
            model=model_name,
            messages=[
                {
                    "role": "system",
                    "content": "You are a helpful assistant that summarizes text. Provide a concise summary.",
                },
                {"role": "user", "content": f"Please summarize the following text:\n\n{text}"},
            ],
        )
        return response.choices[0].message.content
    except Exception as e:
        print(f"Error getting summary from OpenAI: {e}")
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(description="Summarize text from a file or string using OpenAI.")
    parser.add_argument("input", help="File path or text string to summarize")
    parser.add_argument("--text", action="store_true", help="Treat input as direct text instead of a file path")
    parser.add_argument("--model", help="Model alias to use for summarization")
    args = parser.parse_args()

    # Initialize Foundry Local without bootstrapping for visibility
    print("Initializing Foundry Local...")
    fl_manager = FoundryLocalManager(bootstrap=False)
    fl_manager.start_service()
    print("✓ Service started")

    # Check what's available in cache
    cached_models = fl_manager.list_cached_models()

    if args.model:
        # User specified a model — check cache, download if needed
        model_info = fl_manager.get_model_info(args.model)
        if model_info is None:
            print(f"✗ Model alias '{args.model}' not found in catalog")
            sys.exit(1)

        # Check if *any* variant of this alias is already cached
        cached_variant = next((m for m in cached_models if m.alias == args.model), None)
        if cached_variant is not None:
            print(f"✓ Model \"{args.model}\" ({cached_variant.id}) already cached — skipping download")
            model_name = cached_variant.id
        else:
            print(f"Model \"{args.model}\" not in cache. Downloading {model_info.id}...")
            fl_manager.download_model(args.model)
            print("✓ Model downloaded")
            model_name = model_info.id

        print(f"Loading model {model_name}...")
        fl_manager.load_model(model_name)
    else:
        # No model specified — use the first cached model, or fail
        if not cached_models:
            print("No downloaded models available. Run with --model <alias> to download one.")
            sys.exit(1)

        model_name = cached_models[0].id
        print(f"✓ Using cached model: {cached_models[0].alias} ({model_name})")
        # Load by model ID to guarantee we load the exact cached variant
        fl_manager.load_model(model_name)

    print(f"✓ Model loaded and ready\n")

    # Initialize OpenAI client
    client = OpenAI(base_url=fl_manager.endpoint, api_key=fl_manager.api_key)

    # Get input text
    if args.text:
        text = args.input
    else:
        text = read_file_content(args.input)

    # Get and print summary
    summary = get_summary(text, client, model_name)
    print("Summary:")
    print("-" * 50)
    print(summary)
    print("-" * 50)


if __name__ == "__main__":
    main()
