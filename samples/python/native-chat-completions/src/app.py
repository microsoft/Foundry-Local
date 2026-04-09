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
    model.download(
        lambda progress: print(
            f"\rDownloading model: {progress:.2f}%",
            end="",
            flush=True,
        )
    )
    print()
    model.load()
    print("Model loaded and ready.")

    # Get a chat client
    client = model.get_chat_client()
    # </init>

    # <streaming>
    # Create the conversation messages
    messages = [
        {"role": "user", "content": "What is the golden ratio?"}
    ]

    # Stream the response token by token
    print("Assistant: ", end="", flush=True)
    for chunk in client.complete_streaming_chat(messages):
        content = chunk.choices[0].delta.content
        if content:
            print(content, end="", flush=True)
    print()
    # </streaming>

    # Clean up
    model.unload()
    print("Model unloaded.")


if __name__ == "__main__":
    asyncio.run(main())
# </complete_code>
