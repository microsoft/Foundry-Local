# <complete_code>
# <imports>
import asyncio
from foundry_local_sdk import Configuration, FoundryLocalManager
# </imports>


async def main():
    # <init>
    # Initialize the Foundry Local SDK
    config = Configuration(app_name="foundry_local_samples")
    FoundryLocalManager.initialize(config)
    manager = FoundryLocalManager.instance

    # Download and register all execution providers.
    manager.download_and_register_eps()

    # Select and load a model from the catalog
    model = manager.catalog.get_model("qwen2.5-0.5b")
    model.download(lambda progress: print(f"\rDownloading model: {progress:.2f}%", end="", flush=True))
    print()
    model.load()
    print("Model loaded and ready.")

    # Get a chat client
    client = model.get_chat_client()
    # </init>

    # <system_prompt>
    # Start the conversation with a system prompt
    messages = [
        {
            "role": "system",
            "content": "You are a helpful, friendly assistant. Keep your responses "
                       "concise and conversational. If you don't know something, say so."
        }
    ]
    # </system_prompt>

    print("\nChat assistant ready! Type 'quit' to exit.\n")

    # <conversation_loop>
    while True:
        user_input = input("You: ")
        if user_input.strip().lower() in ("quit", "exit"):
            break

        # Add the user's message to conversation history
        messages.append({"role": "user", "content": user_input})

        # <streaming>
        # Stream the response token by token
        print("Assistant: ", end="", flush=True)
        full_response = ""
        for chunk in client.complete_streaming_chat(messages):
            content = chunk.choices[0].message.content
            if content:
                print(content, end="", flush=True)
                full_response += content
        print("\n")
        # </streaming>

        # Add the complete response to conversation history
        messages.append({"role": "assistant", "content": full_response})
    # </conversation_loop>

    # Clean up - unload the model
    model.unload()
    print("Model unloaded. Goodbye!")


if __name__ == "__main__":
    asyncio.run(main())
# </complete_code>
