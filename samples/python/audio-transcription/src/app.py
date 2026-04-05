# <complete_code>
# <imports>
import sys
from pathlib import Path
from foundry_local_sdk import Configuration, FoundryLocalManager
# </imports>


# <init>
# Initialize the Foundry Local SDK
config = Configuration(app_name="foundry_local_samples")
FoundryLocalManager.initialize(config)
manager = FoundryLocalManager.instance

# Load the whisper model for speech-to-text
model = manager.catalog.get_model("whisper-tiny")
model.download(
    lambda progress: print(
        f"\rDownloading model: {progress:.2f}%",
        end="",
        flush=True,
    )
)
print()
model.load()
print("Model loaded.")
# </init>

# <transcription>
# Get the audio client and transcribe
audio_client = model.get_audio_client()
default_audio_file = Path(__file__).resolve().parents[3] / "assets" / "audio" / "Recording.mp3"
audio_file = sys.argv[1] if len(sys.argv) > 1 else str(default_audio_file)
result = audio_client.transcribe(audio_file)
print("Transcription:")
print(result.text)
# </transcription>

# Clean up
model.unload()
# </complete_code>
