# <complete_code>
# <imports>
import asyncio
import openai
from foundry_local_sdk import Configuration, FoundryLocalManager
# </imports>


async def main():
    # <init>
    # Initialize the Foundry Local SDK
    config = Configuration(app_name="foundry_local_samples")
    await FoundryLocalManager.initialize(config)
    manager = FoundryLocalManager.instance

    # Download and register all execution providers.
    current_ep = ""
    def _ep_progress(ep_name: str, percent: float):
        nonlocal current_ep
        if ep_name != current_ep:
            if current_ep:
                print()
            current_ep = ep_name
        print(f"\r  {ep_name:<30}  {percent:5.1f}%", end="", flush=True)

    await manager.download_and_register_eps(progress_callback=_ep_progress)
    if current_ep:
        print()

    # Load a model
    model = await manager.catalog.get_model("qwen2.5-0.5b")
    await model.download(
        lambda progress: print(
            f"\rDownloading model: {progress:.2f}%",
            end="",
            flush=True,
        )
    )
    print()
    await model.load()
    print("Model loaded.")

    # Start the web service to expose an OpenAI-compatible REST endpoint
    await manager.start_web_service()
    base_url = f"{manager.urls[0]}/v1"
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
    await model.unload()
    await manager.stop_web_service()


if __name__ == "__main__":
    asyncio.run(main())
# </complete_code>
