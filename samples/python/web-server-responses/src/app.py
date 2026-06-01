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
    # Track stored response IDs so we can delete them in cleanup. The server
    # caches one inference session per stored response to support
    # `previous_response_id` chaining; without deletion the model holds
    # session references and `model.unload()` fails with
    # "N session(s) still using it".
    stored_response_ids: list[str] = []

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
    # Small models reliably emit a tool call only when the function has concrete
    # parameters that the prompt clearly supplies. Mirror the chat-completions
    # tutorial: a `location` argument plus a prompt that names the location.
    tools = [
        {
            "type": "function",
            "name": "get_weather",
            "description": "Get the current weather for a location.",
            "parameters": {
                "type": "object",
                "properties": {
                    "location": {
                        "type": "string",
                        "description": "The city or location",
                    },
                },
                "required": ["location"],
                "additionalProperties": False,
            },
        },
    ]

    tool_response = openai.responses.create(
        model=model.id,
        input="What's the weather in Seattle? Use the get_weather tool.",
        tools=tools,
        tool_choice="required",
        store=True,
    )
    stored_response_ids.append(tool_response.id)

    function_call = next(
        (item for item in getattr(tool_response, "output", []) or [] if getattr(item, "type", None) == "function_call"),
        None,
    )
    if function_call is None:
        raise RuntimeError("Expected the model to call get_weather.")

    print(f"[TOOL CALL]: {function_call.name}({function_call.arguments})")

    try:
        args = json.loads(function_call.arguments or "{}")
    except json.JSONDecodeError:
        args = {}
    location = args.get("location", "Seattle")

    # No `store=True` here: nothing chains off this response, so there is no
    # reason to keep it (and an extra stored response means an extra cached
    # session the cleanup loop has to release before `model.unload()`).
    final_response = openai.responses.create(
        model=model.id,
        previous_response_id=tool_response.id,
        input=[
            {
                "type": "function_call_output",
                "call_id": function_call.call_id,
                "output": json.dumps({"location": location, "weather": "72 degrees F and sunny"}),
            }
        ],
        tools=tools,
    )

    print(f"[ASSISTANT FINAL]: {get_response_text(final_response)}")
    # <<<<<< END OPENAI SDK USAGE >>>>>>
finally:
    # Tidy up. Delete stored responses first so the server releases the
    # sessions it cached for `previous_response_id` chaining; otherwise
    # `model.unload()` below fails because the model still has live sessions.
    for response_id in stored_response_ids:
        try:
            openai.responses.delete(response_id)
        except Exception as exc:
            print(f"[warning] failed to delete stored response {response_id}: {exc}")
    openai.close()
    manager.stop_web_service()
    model.unload()
# </complete_code>
