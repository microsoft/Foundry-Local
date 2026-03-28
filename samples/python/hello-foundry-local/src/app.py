# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

import sys
import openai
from foundry_local import FoundryLocalManager

# By using an alias, the most suitable model will be downloaded
# to your end-user's device.
alias = "qwen2.5-coder-0.5b"

# Create a FoundryLocalManager instance without bootstrapping
# so we can show each step visually.
print("Initializing Foundry Local...")
manager = FoundryLocalManager(bootstrap=False)
manager.start_service()
print("✓ Service started")

# Check if the model is already cached (downloaded)
cached_models = manager.list_cached_models()
model_info = manager.get_model_info(alias)
if model_info is None:
    print(f"✗ Model \"{alias}\" not found in catalog")
    sys.exit(1)

# Check if *any* variant of this alias is already cached
cached_variant = next((m for m in cached_models if m.alias == alias), None)
if cached_variant is not None:
    print(f"✓ Model \"{alias}\" ({cached_variant.id}) already cached — skipping download")
    model_id = cached_variant.id
else:
    print(f"Model \"{alias}\" not found in cache. Downloading {model_info.id}...")
    manager.download_model(alias)
    print(f"✓ Model downloaded")
    model_id = model_info.id

# Load the model into memory — use the exact model ID to guarantee
# we load the variant that is actually cached.
print(f"Loading model {model_id}...")
manager.load_model(model_id)
print("✓ Model loaded and ready")

# Configure the OpenAI client to use the local Foundry service
client = openai.OpenAI(
    base_url=manager.endpoint,
    api_key=manager.api_key,  # API key is not required for local usage
)

# Generate a streaming response
stream = client.chat.completions.create(
    model=model_id,
    messages=[{"role": "user", "content": "What is the golden ratio?"}],
    stream=True,
)

# Print the streaming response
print("\nAssistant: ", end="")
for chunk in stream:
    if chunk.choices[0].delta.content is not None:
        print(chunk.choices[0].delta.content, end="", flush=True)
print()
