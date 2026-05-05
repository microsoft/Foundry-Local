# <complete_code>
# <imports>
import json

from foundry_local_sdk import Configuration, FoundryLocalManager
# </imports>


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
print("Web service started")
# </server_setup>

# <responses_client>
# Create a Responses API client via the SDK manager — no manual URL or API key needed.
client = manager.create_responses_client(model.id)
# </responses_client>

try:
    print("\nTesting a non-streaming Responses call...")
    response = client.create("Reply with one short sentence about local AI.")
    print(f"[ASSISTANT]: {response.output_text}")

    print("\nTesting a streaming Responses call...")
    print("[ASSISTANT STREAM]: ", end="", flush=True)
    client.create_streaming(
        "Count from one to three.",
        callback=lambda event: print(
            getattr(event, "delta", ""),
            end="",
            flush=True,
        ) if getattr(event, "type", None) == "response.output_text.delta" else None,
    )
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

    tool_response = client.create(
        "Use the get_weather tool and then answer with the weather.",
        tools=tools,
        tool_choice="required",
        store=True,
    )

    function_call = next(
        (item for item in tool_response.output if item.type == "function_call"),
        None,
    )
    if function_call is None:
        raise RuntimeError("Expected the model to call get_weather.")

    print(f"[TOOL CALL]: {function_call.name}({function_call.arguments})")

    final_response = client.create(
        [
            {
                "type": "function_call_output",
                "call_id": function_call.call_id,
                "output": json.dumps({"location": "Seattle", "weather": "72 degrees F and sunny"}),
            }
        ],
        previous_response_id=tool_response.id,
        tools=tools,
    )

    print(f"[ASSISTANT FINAL]: {final_response.output_text}")
finally:
    # Tidy up
    client.close()
    manager.stop_web_service()
    model.unload()
# </complete_code>
