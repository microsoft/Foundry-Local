# <complete_code>
# <imports>
import argparse
import base64
import io
import sys
from pathlib import Path

from PIL import Image
from openai import OpenAI

from foundry_local_sdk import Configuration, FoundryLocalManager
# </imports>


DEFAULT_MODEL = "qwen3.5-9b-cuda-gpu:2"
DEFAULT_IMAGE_PATH = Path(__file__).with_name("test_image.jpg")
DEFAULT_PROMPT = (
    "Describe this image in detail. Mention any visible text, objects, and scene context."
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run a vision prompt with a Foundry Local model through the Responses API."
    )
    parser.add_argument(
        "--model",
        default=DEFAULT_MODEL,
        help=f"Model alias or exact model variant ID. Defaults to {DEFAULT_MODEL}.",
    )
    parser.add_argument(
        "--image",
        type=Path,
        default=DEFAULT_IMAGE_PATH,
        help=f"Path to an input image. Defaults to {DEFAULT_IMAGE_PATH}.",
    )
    parser.add_argument(
        "--prompt",
        default=DEFAULT_PROMPT,
        help="Prompt to send with the image.",
    )
    return parser.parse_args()


def resize_and_encode_image(path: Path, max_dim: int = 768) -> tuple[str, str]:
    if not path.is_file():
        raise FileNotFoundError(f"Image file not found: {path}")

    with Image.open(path) as image:
        if max(image.size) > max_dim:
            image.thumbnail((max_dim, max_dim))
            print(f"  Resized image to {image.size[0]}x{image.size[1]}")

        if image.mode not in ("RGB", "L"):
            image = image.convert("RGB")

        buffer = io.BytesIO()
        image.save(buffer, format="JPEG")

    return base64.b64encode(buffer.getvalue()).decode("utf-8"), "image/jpeg"


def get_model_from_catalog(manager: FoundryLocalManager, model_reference: str):
    if ":" in model_reference:
        model = manager.catalog.get_model_variant(model_reference)
    else:
        model = manager.catalog.get_model(model_reference)

    if model is not None:
        return model

    print(f"\nModel '{model_reference}' was not found in the catalog.")
    print("Available qwen3.5 variants:")
    for candidate in manager.catalog.list_models():
        for variant in candidate.variants:
            if "qwen3.5" in variant.id.lower():
                print(f"  - {variant.id}")
    sys.exit(1)


def main() -> None:
    args = parse_args()

    # <init>
    config = Configuration(app_name="foundry_local_samples")
    FoundryLocalManager.initialize(config)
    manager = FoundryLocalManager.instance

    current_ep = ""

    def ep_progress(ep_name: str, percent: float) -> None:
        nonlocal current_ep
        if ep_name != current_ep:
            if current_ep:
                print()
            current_ep = ep_name
        print(f"\r  {ep_name:<30}  {percent:5.1f}%", end="", flush=True)

    print("\nDownloading execution providers:")
    manager.download_and_register_eps(progress_callback=ep_progress)
    if current_ep:
        print()
    # </init>

    # <model_setup>
    model = get_model_from_catalog(manager, args.model)
    runtime = model.info.runtime
    print(f"\nSelected model: {model.id}")
    if runtime is not None:
        print(f"Runtime: {runtime.device_type} via {runtime.execution_provider}")
    if model.input_modalities is not None:
        print(f"Input modalities: {model.input_modalities}")
    if model.info.file_size_mb is not None:
        print(f"Download size: {model.info.file_size_mb} MB")

    if not model.is_cached:
        print(f"\nDownloading model {model.id}...")
        model.download(
            lambda progress: print(f"\rDownloading model: {progress:.2f}%", end="", flush=True)
        )
        print("\nModel downloaded")

    print("\nLoading model...")
    model.load()
    print("Model loaded")
    # </model_setup>

    openai_client = None
    service_started = False
    try:
        # <server_setup>
        print("\nStarting web service...")
        manager.start_web_service()
        service_started = True
        base_url = manager.urls[0].rstrip("/") + "/v1"
        print("Web service started")

        # <<<<<< OPENAI SDK USAGE >>>>>>
        # Use the OpenAI SDK to call the local Foundry web service Responses API.
        openai_client = OpenAI(base_url=base_url, api_key="notneeded")
        # </server_setup>

        # <inference>
        image_path = args.image.expanduser().resolve()
        print(f"\nPreparing image: {image_path}")
        image_b64, media_type = resize_and_encode_image(image_path)

        vision_input = [
            {
                "type": "message",
                "role": "user",
                "content": [
                    {"type": "input_text", "text": args.prompt},
                    {
                        "type": "input_image",
                        "image_data": image_b64,
                        "media_type": media_type,
                    },
                ],
            }
        ]

        print("\nStreaming vision response...")
        stream = openai_client.responses.create(
            model=model.id,
            input="placeholder",
            extra_body={"input": vision_input},
            stream=True,
        )

        print("[ASSISTANT]: ", end="", flush=True)
        for event in stream:
            if getattr(event, "type", None) == "response.output_text.delta":
                print(getattr(event, "delta", ""), end="", flush=True)
        print()
        # </inference>
    finally:
        try:
            if openai_client is not None:
                openai_client.close()
        finally:
            try:
                if service_started:
                    manager.stop_web_service()
            finally:
                model.unload()


if __name__ == "__main__":
    main()
# </complete_code>
