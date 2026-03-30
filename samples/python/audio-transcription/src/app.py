# <complete_code>
# <imports>
from foundry_local import FoundryLocalManager
# </imports>

# <init>
# Initialize with a whisper model alias
alias = "whisper"
manager = FoundryLocalManager(alias)
# </init>

# <transcription>
# Get the audio client
audio_client = manager.get_audio_client(alias)

# Transcribe an audio file
import sys
audio_file = sys.argv[1] if len(sys.argv) > 1 else "audio.wav"
result = audio_client.transcribe(audio_file)
print("Transcription:")
print(result.text)
# </transcription>
# </complete_code>
