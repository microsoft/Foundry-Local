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
cached_ids = {m.id for m in cached_models}
model_info = manager.get_model_info(alias)
if model_info is None:
    print(f"✗ Model \"{alias}\" not found in catalog")
    sys.exit(1)

if model_info.id in cached_ids:
    print(f"✓ Model \"{alias}\" ({model_info.id}) already cached — skipping download")
else:
    print(f"Model \"{alias}\" not found in cache. Downloading {model_info.id}...")
    manager.download_model(alias)
    print(f"✓ Model downloaded")

# Load the model into memory
print(f"Loading model {model_info.id}...")
manager.load_model(alias)
print("✓ Model loaded and ready")

# Configure the OpenAI client to use the local Foundry service
client = openai.OpenAI(
    base_url=manager.endpoint,
    api_key=manager.api_key,  # API key is not required for local usage
)

# Generate a streaming response
stream = client.chat.completions.create(
    model=model_info.id,
    messages=[{"role": "user", "content": "What is the golden ratio?"}],
    stream=True,
)

# Print the streaming response
print("\nAssistant: ", end="")
for chunk in stream:
    if chunk.choices[0].delta.content is not None:
        print(chunk.choices[0].delta.content, end="", flush=True)
print()
