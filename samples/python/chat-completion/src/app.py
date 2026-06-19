# <complete_code>
# <imports>
import openai

from foundry_local_sdk import Configuration, FoundryLocalManager
# </imports>


def main():
    # <init>
    # Initialize the Foundry Local SDK
    config = Configuration(app_name="foundry_local_samples")
    FoundryLocalManager.initialize(config)
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

    manager.download_and_register_eps(progress_callback=ep_progress)
    if current_ep:
        print()

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
    # </init>

    # The same prompt is answered two ways below: native in-process inference
    # and the local OpenAI-compatible web server.
    messages = [
        {"role": "system", "content": "You are a helpful assistant."},
        {"role": "user", "content": "What is the golden ratio?"},
    ]

    # <native_inference>
    # === Native inference ===
    # Run the prompt with the in-process chat client (no web server involved).
    print("\n=== Native inference ===")
    client = model.get_chat_client()

    print("Assistant: ", end="", flush=True)
    for chunk in client.complete_streaming_chat(messages):
        content = chunk.choices[0].delta.content
        if content:
            print(content, end="", flush=True)
    print()
    # </native_inference>

    # <web_server>
    # === Web server (/v1/chat/completions) ===
    # Start the local OpenAI-compatible web server and send the same prompt
    # through the OpenAI Python client.
    print("\n=== Web server (/v1/chat/completions) ===")
    manager.start_web_service()
    base_url = f"{manager.urls[0]}/v1"

    # Use the OpenAI SDK to connect to the local REST endpoint
    openai_client = openai.OpenAI(base_url=base_url, api_key="none")

    print("Assistant: ", end="", flush=True)
    response = openai_client.chat.completions.create(
        model=model.id,
        messages=messages,
        stream=True,
    )
    for chunk in response:
        if chunk.choices[0].delta.content is not None:
            print(chunk.choices[0].delta.content, end="", flush=True)
    print()

    manager.stop_web_service()
    # </web_server>

    # Clean up
    model.unload()
    print("\nModel unloaded.")


if __name__ == "__main__":
    main()
# </complete_code>
