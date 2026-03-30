# <complete_code>
# <imports>
import openai
from foundry_local_sdk import Configuration, FoundryLocalManager
# </imports>

# <init>
# Initialize the Foundry Local SDK
config = Configuration(app_name="foundry_local_samples")
FoundryLocalManager.initialize(config)
manager = FoundryLocalManager.instance

# Load a model
model = manager.catalog.get_model("qwen2.5-0.5b")
model.download(
    lambda progress: print(
        f"\rDownloading model: {progress:.2f}%",
        end="",
        flush=True,
    )
)
print()
model.load()
print("Model loaded.")

# Start the web service to expose an OpenAI-compatible REST endpoint
manager.start_web_service()
base_url = manager.urls[0]
# </init>

# <rest_client>
# Use the OpenAI SDK to connect to the local REST endpoint
client = openai.OpenAI(
    base_url=base_url,
    api_key="none",
)
# </rest_client>

# <chat_completion>
# Make a chat completion request via the REST API
response = client.chat.completions.create(
    model=model.id,
    messages=[
        {"role": "system", "content": "You are a helpful assistant."},
        {"role": "user", "content": "What is the golden ratio?"}
    ],
    stream=True,
)

for chunk in response:
    if chunk.choices[0].delta.content is not None:
        print(chunk.choices[0].delta.content, end="", flush=True)
print()
# </chat_completion>

# Clean up
model.unload()
manager.stop_web_service()
# </complete_code>
