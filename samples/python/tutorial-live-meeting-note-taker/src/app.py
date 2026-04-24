# NOTE: The live audio transcription API is not yet available in the Python SDK.
# The live transcription API calls below are placeholders. [TO VERIFY]

# <complete_code>
# <imports>
import pyaudio
from foundry_local_sdk import Configuration, FoundryLocalManager
# </imports>

# Audio capture settings
RATE = 16000
CHANNELS = 1
FORMAT = pyaudio.paInt16
CHUNK = 1024


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

    # Load the speech-to-text model
    speech_model = manager.catalog.get_model("nemotron-speech-streaming-en-0.6b")
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
    # </init>

    # <microphone_setup>
    # Set up microphone capture
    p = pyaudio.PyAudio()
    stream = p.open(
        format=FORMAT,
        channels=CHANNELS,
        rate=RATE,
        input=True,
        frames_per_buffer=CHUNK,
    )
    # </microphone_setup>

    # <live_transcription>
    # Create audio client and live transcription session
    audio_client = speech_model.get_audio_client()
    session = audio_client.create_live_transcription_session()  # [TO VERIFY]
    session.settings.language = "en"  # [TO VERIFY]

    accumulated_text = ""
    print("Listening... Press Ctrl+C to stop and summarize.\n")
    session.start()  # [TO VERIFY]

    try:
        while True:
            data = stream.read(CHUNK)
            session.append(data)  # [TO VERIFY]

            for result in session.get_transcription_stream():  # [TO VERIFY]
                text = result.content[0].text  # [TO VERIFY]
                if result.is_final:  # [TO VERIFY]
                    print(text)
                    accumulated_text += text + " "
                else:
                    print(f"\r{text}", end="", flush=True)
    except KeyboardInterrupt:
        print("\nStopping transcription...")
    finally:
        session.stop()  # [TO VERIFY]
        stream.stop_stream()
        stream.close()
        p.terminate()
        speech_model.unload()
        print("Speech model unloaded.")
    # </live_transcription>

    # <summarization>
    if not accumulated_text.strip():
        print("No transcription captured. Skipping summarization.")
        return

    print(f"\nFull transcription:\n{accumulated_text.strip()}\n")

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
        {"role": "user", "content": accumulated_text.strip()},
    ]

    response = client.complete_chat(messages)
    summary = response.choices[0].message.content
    print(f"\nSummary:\n{summary}")

    # Clean up
    chat_model.unload()
    print("\nDone. Models unloaded.")
    # </summarization>


if __name__ == "__main__":
    main()
# </complete_code>
