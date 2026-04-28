# <complete_code>
# <imports>
import asyncio
import sys
from pathlib import Path
from foundry_local_sdk import Configuration, FoundryLocalManager
# </imports>


async def summarize_file(client, file_path, system_prompt):
    """Summarize a single file and print the result."""
    content = Path(file_path).read_text(encoding="utf-8")
    messages = [
        {"role": "system", "content": system_prompt},
        {"role": "user", "content": content}
    ]
    response = await client.complete_chat(messages)
    print(response.choices[0].message.content)


async def summarize_directory(client, directory, system_prompt):
    """Summarize all .txt files in a directory."""
    txt_files = sorted(Path(directory).glob("*.txt"))

    if not txt_files:
        print(f"No .txt files found in {directory}")
        return

    for txt_file in txt_files:
        print(f"--- {txt_file.name} ---")
        await summarize_file(client, txt_file, system_prompt)
        print()


async def main():
    # <init>
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

    # Select and load a model from the catalog
    model = await manager.catalog.get_model("qwen2.5-0.5b")
    await model.download(lambda p: print(f"\rDownloading model: {p:.2f}%", end="", flush=True))
    print()
    await model.load()
    print("Model loaded and ready.\n")

    # Get a chat client
    client = model.get_chat_client()
    # </init>

    # <summarization>
    system_prompt = (
        "Summarize the following document into concise bullet points. "
        "Focus on the key points and main ideas."
    )

    # <file_reading>
    target = sys.argv[1] if len(sys.argv) > 1 else "document.txt"
    target_path = Path(target)
    # </file_reading>

    if target_path.is_dir():
        await summarize_directory(client, target_path, system_prompt)
    else:
        print(f"--- {target_path.name} ---")
        await summarize_file(client, target_path, system_prompt)
    # </summarization>

    # Clean up
    await model.unload()
    print("\nModel unloaded. Done!")


if __name__ == "__main__":
    asyncio.run(main())
# </complete_code>
