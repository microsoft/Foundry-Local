# Live Audio Transcription — Foundry Local SDK Example (Python)
#
# NOTE: This sample requires the live-transcription session API
# (create_live_transcription_session) which is not yet available in the
# Python SDK. It is included as a forward-looking reference showing the
# expected API usage. It will not run until the API is added to the SDK.
#
# Demonstrates real-time microphone-to-text using:
#   SDK (FoundryLocalManager) → Core (NativeAOT DLL) → onnxruntime-genai (StreamingProcessor)
#
# Usage (once the API is available):
#   pip install -r requirements.txt
#   python src/app.py

import threading

import pyaudio
from foundry_local_sdk import Configuration, FoundryLocalManager

print("===========================================================")
print("   Foundry Local -- Live Audio Transcription Demo (Python)")
print("===========================================================")
print()

config = Configuration(app_name="foundry_local_samples")
FoundryLocalManager.initialize(config)
manager = FoundryLocalManager.instance

manager.download_and_register_eps()

model = manager.catalog.get_model("nemotron")
if model is None:
    raise RuntimeError('Model "nemotron" not found in catalog')

model.download(
    lambda progress: print(f"\rDownloading model: {progress:.2f}%", end="", flush=True)
)
print()
print(f"Loading model {model.id}...", end="")
model.load()
print("done.")

audio_client = model.get_audio_client()
session = audio_client.create_live_transcription_session()
session.settings.sample_rate = 16000
session.settings.channels = 1
session.settings.language = "en"

session.start()
print("       Session started")


def read_results():
    for result in session.get_transcription_stream():
        text = result.content[0].text if result.content else ""
        if result.is_final:
            print()
            print(f"  [FINAL] {text}")
        elif text:
            print(f"\033[96m{text}\033[0m", end="", flush=True)


read_thread = threading.Thread(target=read_results, daemon=True)
read_thread.start()

rate = 16000
channels = 1
fmt = pyaudio.paInt16
chunk = rate // 10  # 100ms

pa = pyaudio.PyAudio()
stream = pa.open(
    format=fmt,
    channels=channels,
    rate=rate,
    input=True,
    frames_per_buffer=chunk,
)

print()
print("===========================================================")
print("  LIVE TRANSCRIPTION ACTIVE")
print("  Speak into your microphone.")
print("  Transcription appears in real-time (cyan text).")
print("  Press ENTER to stop recording.")
print("===========================================================")
print()

stop_recording = threading.Event()


def capture_mic():
    while not stop_recording.is_set():
        try:
            pcm_data = stream.read(chunk, exception_on_overflow=False)
            if pcm_data:
                session.append(pcm_data)
        except Exception as e:
            print(f"\n[ERROR] Microphone capture failed: {e}")
            stop_recording.set()
            break


capture_thread = threading.Thread(target=capture_mic, daemon=True)
capture_thread.start()

input()

stop_recording.set()
capture_thread.join(timeout=2)

stream.stop_stream()
stream.close()
pa.terminate()

session.stop()
read_thread.join()
model.unload()
