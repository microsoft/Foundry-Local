# <complete_code>
# <imports>
import asyncio
import json
from foundry_local_sdk import Configuration, FoundryLocalManager
# </imports>


# <tool_definitions>
# --- Tool definitions ---
tools = [
    {
        "type": "function",
        "function": {
            "name": "get_weather",
            "description": "Get the current weather for a location",
            "parameters": {
                "type": "object",
                "properties": {
                    "location": {
                        "type": "string",
                        "description": "The city or location"
                    },
                    "unit": {
                        "type": "string",
                        "enum": ["celsius", "fahrenheit"],
                        "description": "Temperature unit"
                    }
                },
                "required": ["location"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "calculate",
            "description": "Perform a math calculation",
            "parameters": {
                "type": "object",
                "properties": {
                    "expression": {
                        "type": "string",
                        "description": (
                            "The math expression to evaluate"
                        )
                    }
                },
                "required": ["expression"]
            }
        }
    }
]


# --- Tool implementations ---
def get_weather(location, unit="celsius"):
    """Simulate a weather lookup."""
    return {
        "location": location,
        "temperature": 22 if unit == "celsius" else 72,
        "unit": unit,
        "condition": "Sunny"
    }


def calculate(expression):
    """Evaluate a math expression safely."""
    allowed = set("0123456789+-*/(). ")
    if not all(c in allowed for c in expression):
        return {"error": "Invalid expression"}
    try:
        result = eval(expression)
        return {"expression": expression, "result": result}
    except Exception as e:
        return {"error": str(e)}


tool_functions = {
    "get_weather": get_weather,
    "calculate": calculate
}
# </tool_definitions>


# <tool_loop>
async def process_tool_calls(messages, response, client):
    """Handle tool calls in a loop until the model produces a final answer."""
    choice = response.choices[0].message

    while choice.tool_calls:
        # Convert the assistant message to a dict for the SDK
        assistant_msg = {
            "role": "assistant",
            "content": choice.content,
            "tool_calls": [
                {
                    "id": tc.id,
                    "type": tc.type,
                    "function": {
                        "name": tc.function.name,
                        "arguments": tc.function.arguments,
                    },
                }
                for tc in choice.tool_calls
            ],
        }
        messages.append(assistant_msg)

        for tool_call in choice.tool_calls:
            function_name = tool_call.function.name
            arguments = json.loads(tool_call.function.arguments)
            print(f"  Tool call: {function_name}({arguments})")

            # Execute the function and add the result
            func = tool_functions[function_name]
            result = func(**arguments)
            messages.append({
                "role": "tool",
                "tool_call_id": tool_call.id,
                "content": json.dumps(result)
            })

        # Send the updated conversation back
        response = await client.complete_chat(messages, tools=tools)
        choice = response.choices[0].message

    return choice.content
# </tool_loop>


# <init>
async def main():
    # Initialize the Foundry Local SDK
    config = Configuration(app_name="foundry_local_samples")
    await FoundryLocalManager.initialize(config)
    manager = FoundryLocalManager.instance

    # Download and register all execution providers.
    current_ep = ""
    def ep_progress(ep_name: str, percent: float):
        nonlocal current_ep
        if ep_name != current_ep:
            if current_ep:
                print()
            current_ep = ep_name
        print(f"\r  {ep_name:<30}  {percent:5.1f}%", end="", flush=True)

    await manager.download_and_register_eps(progress_callback=ep_progress)
    if current_ep:
        print()

    # Select and load a model
    model = await manager.catalog.get_model("qwen2.5-0.5b")
    await model.download(
        lambda progress: print(
            f"\rDownloading model: {progress:.2f}%",
            end="",
            flush=True
        )
    )
    print()
    await model.load()
    print("Model loaded and ready.")

    # Get a chat client
    client = model.get_chat_client()

    # Conversation with a system prompt
    messages = [
        {
            "role": "system",
            "content": "You are a helpful assistant with access to tools. "
                       "Use them when needed to answer questions accurately."
        },
        {
            "role": "user",
            "content": "What is the weather in Seattle and what is 42 * 17?"
        }
    ]

    print("Sending request with tools...")
    response = await client.complete_chat(messages, tools=tools)
    answer = await process_tool_calls(messages, response, client)

    print(f"\nAssistant: {answer}")

    # Clean up
    await model.unload()
    print("Model unloaded.")
# </init>


if __name__ == "__main__":
    asyncio.run(main())
# </complete_code>
