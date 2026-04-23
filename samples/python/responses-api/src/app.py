# <complete_code>
# <imports>
import sys

from foundry_local_sdk import Configuration, FoundryLocalManager
from foundry_local_sdk.openai.responses_client import create_image_content
# </imports>

# ── Configuration ──────────────────────────────────────────────────────────
MODEL_ALIAS = "qwen3.5-4b"


def main():
    # <init>
    # Parse CLI arguments
    text_input = None
    image_path = None
    check_cache = False
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] == "--image" and i + 1 < len(args):
            i += 1
            image_path = args[i]
        elif args[i] == "--check-cache":
            check_cache = True
        elif text_input is None:
            text_input = args[i]
        i += 1

    if not text_input and not check_cache:
        print('Usage: python app.py "your prompt" [--image path/or/url] [--check-cache]')
        print('       python app.py "What is quantum computing?"')
        print('       python app.py "Describe this image" --image photo.png')
        print('       python app.py --check-cache')
        sys.exit(1)

    # Initialize the Foundry Local SDK
    config = Configuration(app_name="foundry_local_samples")
    FoundryLocalManager.initialize(config)
    manager = FoundryLocalManager.instance
    print("SDK initialized.")

    # ── Check Cache ─────────────────────────────────────────────────────
    if check_cache:
        cached = manager.catalog.get_cached_models()
        aliases = sorted(set(m.alias for m in cached))
        if not aliases:
            print("\nNo models in cache.")
        else:
            print(f"\nCached models ({len(aliases)}):")
            for alias in aliases:
                print(f"  - {alias}")
        found = any(m.alias == MODEL_ALIAS or m.id == MODEL_ALIAS for m in cached)
        print(f"\n{MODEL_ALIAS} is {'cached' if found else 'NOT cached'}")
        return

    # Download and register execution providers
    current_ep = ""

    def ep_progress(ep_name: str, percent: float):
        nonlocal current_ep
        if ep_name != current_ep:
            if current_ep:
                print()
            current_ep = ep_name
        print(f"\r  {ep_name:<30}  {percent:5.1f}%", end="", flush=True)

    manager.download_and_register_eps(progress_callback=ep_progress)
    if current_ep:
        print()

    # Select and load a model from the catalog
    model = None
    try:
        model = manager.catalog.get_model(MODEL_ALIAS)
    except Exception:
        cached = manager.catalog.get_cached_models()
        model = next((m for m in cached if m.alias == MODEL_ALIAS or m.id == MODEL_ALIAS), None)
        if model:
            print(f"Model '{MODEL_ALIAS}' not in catalog, using cached version")

    if model is None:
        print(f"Model '{MODEL_ALIAS}' not found in catalog or cache.")
        sys.exit(1)

    # Select CPU variant if multiple variants exist
    if len(model.variants) > 1:
        print("\nModel variants:")
        for v in model.variants:
            print(f"  - {v.id} (cached: {v.is_cached})")
        cpu = next((v for v in model.variants if "cpu" in v.id), None)
        if cpu:
            model.select_variant(cpu)
            print(f"Selected CPU variant: {cpu.id}")

    if not model.is_cached:
        print(f"\nDownloading model {MODEL_ALIAS}...")
        model.download(
            lambda progress: print(
                f"\rDownloading... {progress:.2f}%",
                end="",
                flush=True,
            )
        )
        print()
    else:
        print(f"Model {MODEL_ALIAS} found in cache.")

    print(f"Loading model {MODEL_ALIAS}...")
    model.load()
    print("Model loaded.")
    # </init>

    # <responses_api>
    # Start the web service (required for Responses API)
    manager.start_web_service()
    print(f"Web service started at {manager.urls[0]}")

    # Create a Responses API client
    client = manager.create_responses_client(model.id)
    client.settings.temperature = 0.7
    client.settings.max_output_tokens = 512

    # ── Build input ─────────────────────────────────────────────────────
    if image_path:
        print(f"\nImage: {image_path}")
    print(f"Prompt: {text_input}\n")
    print("--- Response ---")

    content = []
    if image_path:
        content.append(create_image_content(image_path))
    content.append({"type": "input_text", "text": text_input})

    input_items = [{"type": "message", "role": "user", "content": content}]

    # ── Streaming ───────────────────────────────────────────────────────
    for event in client.create_streaming(input_items):
        if event["type"] == "response.output_text.delta":
            print(event["delta"], end="", flush=True)
        elif event["type"] == "response.failed":
            print(f"\n[ERROR] {event.get('response', {}).get('error', event)}")
        elif event["type"] == "response.completed":
            print(f"\n[Status: {event.get('response', {}).get('status', 'unknown')}]")
    print()
    # </responses_api>

    # Clean up
    model.unload()
    manager.stop_web_service()
    print("Done.")


if __name__ == "__main__":
    main()
# </complete_code>
