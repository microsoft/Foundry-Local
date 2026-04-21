# <complete_code>
# <imports>
import asyncio
import sys
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

    # Load the whisper model for speech-to-text
    model = await manager.catalog.get_model("whisper-tiny")
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
    # </init>

    # <transcription>
    # Get the audio client and transcribe
    audio_client = model.get_audio_client()
    audio_file = sys.argv[1] if len(sys.argv) > 1 else "Recording.mp3"
    result = await audio_client.transcribe(audio_file)
    print("Transcription:")
    print(result.text)
    # </transcription>

    # Clean up
    await model.unload()


if __name__ == "__main__":
    asyncio.run(main())
# </complete_code>
