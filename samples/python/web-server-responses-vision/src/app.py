# <complete_code>
# <imports>
import base64
import io
import sys

from PIL import Image
from openai import OpenAI

from foundry_local_sdk import Configuration, FoundryLocalManager
# </imports>

if len(sys.argv) < 2:
    print("Usage: python src/app.py <model_alias> [image_path]")
    print("  Example: python src/app.py qwen3.5-0.8b path/to/image.jpg")
    print("  Text only: python src/app.py qwen3.5-0.8b")
    sys.exit(1)

model_alias = sys.argv[1]
image_path = sys.argv[2] if len(sys.argv) > 2 else None


def resize_and_encode(path, max_dim=512):
    """Load and resize a local image, returning (base64_str, media_type)."""
    img = Image.open(path)
    if max(img.size) > max_dim:
        img.thumbnail((max_dim, max_dim))
        print(f"  (resized to {img.size[0]}x{img.size[1]})")
    buf = io.BytesIO()
    img.save(buf, format="JPEG")
    return base64.b64encode(buf.getvalue()).decode(), "image/jpeg"


# <init>
config = Configuration(app_name="foundry_local_samples")
FoundryLocalManager.initialize(config)
manager = FoundryLocalManager.instance
# </init>

# <model_setup>
model = manager.catalog.get_model(model_alias)
if model is None:
    available = [m.alias for m in manager.catalog.list_models()]
    print(f"\nModel '{model_alias}' not found in catalog.")
    print(f"Available models: {available}")
    sys.exit(1)

if not model.is_cached:
    print(f"\nDownloading model {model_alias}...")
    model.download(
        lambda progress: print(f"\rDownloading model: {progress:.2f}%", end="", flush=True)
    )
    print("\nModel downloaded")

print("\nLoading model...")
model.load()
print("Model loaded")
# </model_setup>

# <server_setup>
print("\nStarting web service...")
manager.start_web_service()
base_url = manager.urls[0].rstrip("/") + "/v1"
print("Web service started")

# <<<<<< OPENAI SDK USAGE >>>>>>
# Use the OpenAI SDK to call the local Foundry web service Responses API
openai = OpenAI(base_url=base_url, api_key="notneeded")
# </server_setup>

# <inference>
if image_path:
    print(f"\nPreparing image: {image_path}")
    image_b64, media_type = resize_and_encode(image_path)

    vision_input = [
        {
            "type": "message",
            "role": "user",
            "content": [
                {"type": "input_text", "text": "Describe this image."},
                {
                    "type": "input_image",
                    "image_data": image_b64,
                    "media_type": media_type,
                },
            ],
        }
    ]

    print("\nStreaming vision response...")
    stream = openai.responses.create(
        model=model.id,
        input="placeholder",
        extra_body={"input": vision_input},
        stream=True,
    )
else:
    print("\nStreaming text response...")
    stream = openai.responses.create(
        model=model.id,
        input="Reply with one short sentence about local AI.",
        stream=True,
    )

print("[ASSISTANT]: ", end="", flush=True)
for event in stream:
    if getattr(event, "type", None) == "response.output_text.delta":
        print(getattr(event, "delta", ""), end="", flush=True)
print()
# </inference>

openai.close()
manager.stop_web_service()
model.unload()
