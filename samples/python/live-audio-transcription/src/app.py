# Live Audio Transcription — Foundry Local SDK Example (Python)
#
# Demonstrates real-time microphone-to-text using:
#   SDK (FoundryLocalManager) → Core (NativeAOT DLL) → onnxruntime-genai (StreamingProcessor)
#
# Usage:
#   pip install pyaudio
#   python app.py

import threading

import pyaudio
from foundry_local_sdk import Configuration, FoundryLocalManager

print("===========================================================")
print("   Foundry Local -- Live Audio Transcription Demo (Python)")
print("===========================================================")
print()

# Initialize
config = Configuration(app_name="foundry_local_samples")
FoundryLocalManager.initialize(config)
manager = FoundryLocalManager.instance

# Download and load the Nemotron ASR model
model = manager.catalog.get_model("nemotron")
model.download(
    lambda progress: print(
        f"\rDownloading model: {progress:.2f}%", end="", flush=True
    )
)
print()
print(f"Loading model {model.id}...", end="")
model.load()
print("done.")

# Create a live transcription session
audio_client = model.get_audio_client()
session = audio_client.create_live_transcription_session()
session.settings.sample_rate = 16000
session.settings.channels = 1
session.settings.language = "en"

session.start()
print("       Session started")

# Start reading transcription results in a background thread
def read_results():
    try:
        for result in session.get_transcription_stream():
            text = result.content[0].text if result.content else ""
            if result.is_final:
                print()
                print(f"  [FINAL] {text}")
            elif text:
                print(f"\033[96m{text}\033[0m", end="", flush=True)
    except Exception:
        pass

read_thread = threading.Thread(target=read_results, daemon=True)
read_thread.start()

# Open microphone with PyAudio
RATE = 16000
CHANNELS = 1
FORMAT = pyaudio.paInt16
CHUNK = RATE // 10  # 100ms chunks

pa = pyaudio.PyAudio()
stream = pa.open(
    format=FORMAT,
    channels=CHANNELS,
    rate=RATE,
    input=True,
    frames_per_buffer=CHUNK,
)

print()
print("===========================================================")
print("  LIVE TRANSCRIPTION ACTIVE")
print("  Speak into your microphone.")
print("  Transcription appears in real-time (cyan text).")
print("  Press ENTER to stop recording.")
print("===========================================================")
print()

# Capture microphone audio in a background thread, push to session
stop_recording = threading.Event()

def capture_mic():
    """Read PCM chunks from the microphone and push to the streaming session."""
    while not stop_recording.is_set():
        try:
            pcm_data = stream.read(CHUNK, exception_on_overflow=False)
            if pcm_data:
                session.append(pcm_data)
        except Exception:
            break

capture_thread = threading.Thread(target=capture_mic, daemon=True)
capture_thread.start()

# Wait for ENTER to stop
input()

# Stop recording
stop_recording.set()
capture_thread.join(timeout=2)

stream.stop_stream()
stream.close()
pa.terminate()

session.stop()
read_thread.join()

model.unload()
