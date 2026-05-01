# <complete_code>
# <imports>
import json
from typing import Any

from openai import OpenAI

from foundry_local_sdk import Configuration, FoundryLocalManager
# </imports>


def get_response_text(response: Any) -> str:
    if isinstance(getattr(response, "output_text", None), str):
        return response.output_text
    return "".join(
        getattr(part, "text", "")
        for item in getattr(response, "output", []) or []
        for part in getattr(item, "content", []) or []
        if getattr(part, "type", None) == "output_text"
    )


# <init>
# Initialize the Foundry Local SDK
config = Configuration(app_name="foundry_local_samples")
FoundryLocalManager.initialize(config)
manager = FoundryLocalManager.instance

# Download and register all execution providers.
current_ep = ""


def _ep_progress(ep_name: str, percent: float):
    global current_ep
    if ep_name != current_ep:
        if current_ep:
            print()
        current_ep = ep_name
    print(f"\r  {ep_name:<30}  {percent:5.1f}%", end="", flush=True)


manager.download_and_register_eps(progress_callback=_ep_progress)
if current_ep:
    print()
# </init>

# <model_setup>
model_alias = "qwen2.5-0.5b"
model = manager.catalog.get_model(model_alias)

print(f"\nDownloading model {model_alias}...")
model.download(
    lambda progress: print(
        f"\rDownloading model: {progress:.2f}%",
        end="",
        flush=True,
    )
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
openai = OpenAI(
    base_url=base_url,
    api_key="notneeded",
)
# </server_setup>

try:
    print("\nTesting a non-streaming Responses call...")
    response = openai.responses.create(
        model=model.id,
        input="Reply with one short sentence about local AI.",
    )
    print(f"[ASSISTANT]: {get_response_text(response)}")

    print("\nTesting a streaming Responses call...")
    stream = openai.responses.create(
        model=model.id,
        input="Count from one to three.",
        stream=True,
    )

    print("[ASSISTANT STREAM]: ", end="", flush=True)
    for event in stream:
        if getattr(event, "type", None) == "response.output_text.delta":
            print(getattr(event, "delta", ""), end="", flush=True)
    print()

    print("\nTesting Responses tool calling...")
    tools = [
        {
            "type": "function",
            "name": "get_weather",
            "description": "Get the current weather. This sample always returns Seattle weather.",
            "parameters": {
                "type": "object",
                "properties": {},
                "additionalProperties": False,
            },
        },
    ]

    tool_response = openai.responses.create(
        model=model.id,
        input="Use the get_weather tool and then answer with the weather.",
        tools=tools,
        tool_choice="required",
        store=True,
    )

    function_call = next(
        (item for item in getattr(tool_response, "output", []) or [] if getattr(item, "type", None) == "function_call"),
        None,
    )
    if function_call is None:
        raise RuntimeError("Expected the model to call get_weather.")

    print(f"[TOOL CALL]: {function_call.name}({function_call.arguments})")

    final_response = openai.responses.create(
        model=model.id,
        previous_response_id=tool_response.id,
        input=[
            {
                "type": "function_call_output",
                "call_id": function_call.call_id,
                "output": json.dumps({"location": "Seattle", "weather": "72 degrees F and sunny"}),
            }
        ],
        tools=tools,
    )

    print(f"[ASSISTANT FINAL]: {get_response_text(final_response)}")
    # <<<<<< END OPENAI SDK USAGE >>>>>>
finally:
    # Tidy up
    openai.close()
    manager.stop_web_service()
    model.unload()
# </complete_code>
