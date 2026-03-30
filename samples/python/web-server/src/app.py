# <complete_code>
# <imports>
from foundry_local import FoundryLocalManager
import openai
# </imports>

# <init>
alias = "qwen2.5-0.5b"
manager = FoundryLocalManager(alias)
# </init>

# <rest_client>
# Use the OpenAI SDK to connect to the local REST endpoint
client = openai.OpenAI(
    base_url=manager.endpoint,
    api_key=manager.api_key,
)
# </rest_client>

# <chat_completion>
# Make a chat completion request via the REST API
response = client.chat.completions.create(
    model=manager.get_model_info(alias).id,
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
# </complete_code>
