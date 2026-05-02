# Live Audio Transcription — Foundry Local SDK Example (Python)
#
# Tries PyAudio mic capture first; falls back to synthetic PCM if unavailable.
#
# Usage:
#   pip install -r requirements.txt
#   python src/app.py              # Live microphone
#   python src/app.py --synth      # Synthetic 440Hz sine wave

import math
import signal
import struct
import sys
import threading
import time

from foundry_local_sdk import Configuration, FoundryLocalManager
from foundry_local_sdk.exception import FoundryLocalException
from foundry_local_sdk.openai.live_audio_transcription_types import CoreErrorResponse

use_synth = "--synth" in sys.argv

print("===========================================================")
print("   Foundry Local -- Live Audio Transcription Demo (Python)")
print("===========================================================")
print()

config = Configuration(app_name="foundry_local_samples")
FoundryLocalManager.initialize(config)
manager = FoundryLocalManager.instance

manager.download_and_register_eps()

model = manager.catalog.get_model("nemotron-speech-streaming-en-0.6b")
if model is None:
    raise RuntimeError('Model "nemotron-speech-streaming-en-0.6b" not found in catalog')

model.download(
    lambda progress: print(f"\rDownloading model: {progress:.2f}%", end="", flush=True)
)
print()
print(f"Loading model {model.id}...", end="")
model.load()
print("done.")

# Graceful-shutdown coordinator. Set ONCE on the session via
# create_live_transcription_session(cancel_event=...) — every subsequent
# start() / append() / stop() / get_transcription_stream() call picks it
# up automatically, so we don't have to thread the event through every
# callsite. SIGINT just calls shutdown_event.set() and the in-flight
# session work unwinds cleanly.
shutdown_event = threading.Event()

audio_client = model.get_audio_client()
session = audio_client.create_live_transcription_session(cancel_event=shutdown_event)
session.settings.sample_rate = 16000
session.settings.channels = 1
session.settings.language = "en"

session.start()
print("✓ Session started")

# --- Background thread reads transcription results (mirrors JS readPromise) ---


def read_results():
    try:
        for result in session.get_transcription_stream():
            text = result.content[0].text if result.content else ""
            if result.is_final:
                print()
                print(f"  [FINAL] {text}")
            elif text:
                print(text, end="", flush=True)
    except FoundryLocalException as ex:
        # Cancelled via shutdown_event -> generator returns cleanly (no exception).
        # We only land here on a real native-side push failure.
        # Use CoreErrorResponse to inspect structured error metadata (code +
        # is_transient) and decide whether to retry or surface the error.
        # Without it, the only signal would be str(ex).
        info = CoreErrorResponse.try_parse(str(ex))
        if info and info.is_transient:
            print(f"\n⚠ Transient ASR error ({info.code}): {info.message}. Continuing...")
            return
        if info:
            print(f"\n✗ Stream error [{info.code}]: {info.message}")
            return
        print(f"\n✗ Stream error: {ex}")


read_thread = threading.Thread(target=read_results, daemon=True)
read_thread.start()

# --- Microphone capture (mirrors JS naudiodon2 / C++ PortAudio) ---
# Try PyAudio for mic input; fall back to synthetic PCM on failure.

RATE = 16000
CHANNELS = 1
CHUNK = RATE // 10  # 100ms of audio = 1600 frames

mic_active = False
pa = None
stream = None

if not use_synth:
    try:
        import pyaudio

        pa = pyaudio.PyAudio()
        stream = pa.open(
            format=pyaudio.paInt16,
            channels=CHANNELS,
            rate=RATE,
            input=True,
            frames_per_buffer=CHUNK,
        )
        mic_active = True

        print()
        print("===========================================================")
        print("  LIVE TRANSCRIPTION ACTIVE")
        print("  Speak into your microphone.")
        print("  Press Ctrl+C to stop.")
        print("===========================================================")
        print()

        def capture_mic():
            while not shutdown_event.is_set():
                try:
                    pcm_data = stream.read(CHUNK, exception_on_overflow=False)
                    if pcm_data:
                        # Session-level cancel_event applies — if shutdown
                        # fires while append() is blocked on backpressure,
                        # it raises FoundryLocalException("cancelled") instead
                        # of waiting for the queue to drain.
                        session.append(pcm_data)
                except FoundryLocalException:
                    # Session was cancelled — exit the capture loop cleanly.
                    break
                except Exception as e:
                    print(f"\n[ERROR] Microphone capture failed: {e}")
                    shutdown_event.set()
                    break

        capture_thread = threading.Thread(target=capture_mic, daemon=True)
        capture_thread.start()

    except Exception as e:
        print(f"Could not initialize microphone: {e}")
        print("Falling back to synthetic audio test...")
        print()
        mic_active = False
        if stream:
            stream.close()
        if pa:
            pa.terminate()
        pa = None
        stream = None

# Fallback: push synthetic PCM (440Hz sine wave) — mirrors JS catch block
if not mic_active:
    print("Pushing synthetic audio (440Hz sine, 2s)...")
    duration = 2
    total_samples = RATE * duration
    pcm_bytes = bytearray(total_samples * 2)
    for i in range(total_samples):
        t = i / RATE
        sample = int(32767 * 0.5 * math.sin(2 * math.pi * 440 * t))
        struct.pack_into("<h", pcm_bytes, i * 2, sample)

    chunk_size = (RATE // 10) * 2  # 100ms
    for offset in range(0, len(pcm_bytes), chunk_size):
        end = min(offset + chunk_size, len(pcm_bytes))
        session.append(bytes(pcm_bytes[offset:end]))
        time.sleep(0.1)

    print("✓ Synthetic audio pushed")
    time.sleep(3)  # Wait for remaining transcription results


# --- Graceful shutdown (mirrors JS SIGINT handler / C++ SignalHandler) ---


def shutdown(*_args):
    print("\n\nStopping...")
    # Setting shutdown_event:
    #   - exits the mic capture loop on its next iteration
    #   - aborts any in-flight session.append() blocked on backpressure
    #     with FoundryLocalException("cancelled")
    #   - ends session.get_transcription_stream() iteration cleanly in
    #     the read thread
    #   - short-circuits session.stop()'s drain wait below
    shutdown_event.set()

    if stream:
        stream.stop_stream()
        stream.close()
    if pa:
        pa.terminate()

    session.stop()
    read_thread.join(timeout=5)
    model.unload()
    print("✓ Done")
    sys.exit(0)


signal.signal(signal.SIGINT, lambda *a: shutdown())

if mic_active:
    # Block until Ctrl+C
    shutdown_event.wait()
else:
    shutdown()
