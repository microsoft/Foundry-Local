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
    # </init>

    # <transcription>
    # Load the speech-to-text model
    speech_model = manager.catalog.get_model("whisper-tiny")
    speech_model.download(
        lambda progress: print(
            f"\rDownloading speech model: {progress:.2f}%",
            end="",
            flush=True,
        )
    )
    print()
    speech_model.load()
    print("Speech model loaded.")

    # Transcribe the audio file
    audio_client = speech_model.get_audio_client()
    transcription = audio_client.transcribe("meeting-notes.wav")
    print(f"\nTranscription:\n{transcription.text}")

    # Unload the speech model to free memory
    speech_model.unload()
    # </transcription>

    # <summarization>
    # Load the chat model for summarization
    chat_model = manager.catalog.get_model("qwen2.5-0.5b")
    chat_model.download(
        lambda progress: print(
            f"\rDownloading chat model: {progress:.2f}%",
            end="",
            flush=True,
        )
    )
    print()
    chat_model.load()
    print("Chat model loaded.")

    # Summarize the transcription into organized notes
    client = chat_model.get_chat_client()
    messages = [
        {
            "role": "system",
            "content": "You are a note-taking assistant. "
                       "Summarize the following transcription "
                       "into organized, concise notes with "
                       "bullet points.",
        },
        {"role": "user", "content": transcription.text},
    ]

    response = client.complete_chat(messages)
    summary = response.choices[0].message.content
    print(f"\nSummary:\n{summary}")

    # Clean up
    chat_model.unload()
    print("\nDone. Models unloaded.")
    # </summarization>


if __name__ == "__main__":
    asyncio.run(main())
# </complete_code>
