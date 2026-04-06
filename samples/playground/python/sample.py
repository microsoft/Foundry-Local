import sys
import os

from foundry_local_sdk import Configuration, FoundryLocalManager
from ui import (section, show_ep_table, show_catalog, create_download_bar,
                print_user_msg, create_stream_box, ask_user)

# ── Initialize ───────────────────────────────────────────────────────────

config = Configuration(app_name="foundry-local-playground")
FoundryLocalManager.initialize(config)
manager = FoundryLocalManager.instance

# ── Discover & download execution providers ──────────────────────────────

section("Execution Providers")

eps = manager.discover_eps()
ep_ui = show_ep_table(eps)

unregistered = [ep for ep in eps if not ep.is_registered]
if unregistered:
    result = manager.download_and_register_eps(
        names=[ep.name for ep in unregistered],
        progress_callback=ep_ui["on_progress"],
    )
    ep_ui["finalize"](result)

# ── Browse model catalog & pick a model ──────────────────────────────────

section("Model Catalog")

models = manager.catalog.list_models()
models.sort(key=lambda m: (not m.is_cached, m.alias))
for m in models:
    m.variants.sort(key=lambda v: (not v.is_cached, v.id))

rows = show_catalog(models)

choice = ask_user(f"\n  Select a model [\x1b[36m1-{len(rows)}\x1b[0m]: ")
try:
    selected_idx = int(choice) - 1
    assert 0 <= selected_idx < len(rows)
except (ValueError, AssertionError):
    print("  Invalid selection.")
    sys.exit(1)

chosen = rows[selected_idx]
model_alias = chosen["model"].alias
print(f"\n  Selected: \x1b[32m{model_alias}\x1b[0m ({chosen['variant'].id})")

# ── Download & load the model ────────────────────────────────────────────

model = manager.catalog.get_model(model_alias)
model.select_variant(chosen["variant"])
section(f"Model – {model_alias}")

if not model.is_cached:
    dl = create_download_bar(model_alias)
    model.download(progress_callback=dl["on_progress"])
    dl["finalize"]()

model.load()
print("  \x1b[32m✓\x1b[0m Model loaded\n")

# ── Detect task type ─────────────────────────────────────────────────────

task = (chosen["model"].info.task or "").lower()
is_audio = ("speech-recognition" in task or "speech-to-text" in task
            or "whisper" in model_alias.lower())

if is_audio:
    # ── Audio Transcription ──────────────────────────────────────────────

    section("Audio Transcription  (enter a file path, /quit to exit)")

    audio_client = model.get_audio_client()

    while True:
        user_input = ask_user("  \x1b[36maudio file> \x1b[0m")
        trimmed = user_input.strip()
        if not trimmed:
            continue
        if trimmed in ("/quit", "/exit", "/q"):
            break

        audio_path = os.path.abspath(trimmed)
        print(f"  {audio_path}\n")

        box = create_stream_box()
        try:
            audio_client.transcribe_streaming(audio_path, lambda chunk: [
                box["write"](c) for c in (chunk.text or "")
            ])
        except Exception as err:
            box["finish"]()
            print(f"  \x1b[31mError: {err}\x1b[0m\n")
            continue
        box["finish"]()

else:
    # ── Interactive Chat ─────────────────────────────────────────────────

    section("Chat  (type a message, /quit to exit)")

    chat_client = model.get_chat_client()
    messages = [{"role": "system", "content": "You are a helpful assistant."}]

    while True:
        user_input = ask_user()
        trimmed = user_input.strip()
        if not trimmed:
            continue
        if trimmed in ("/quit", "/exit", "/q"):
            break

        sys.stdout.write("\x1b[1A\r\x1b[K")
        sys.stdout.flush()
        print_user_msg(trimmed)

        messages.append({"role": "user", "content": trimmed})

        box = create_stream_box()
        response = ""

        for chunk in chat_client.complete_streaming_chat(messages):
            content = None
            if chunk.choices and chunk.choices[0].delta:
                content = chunk.choices[0].delta.content
            if content:
                response += content
                for char in content:
                    box["write"](char)

        box["finish"]()
        messages.append({"role": "assistant", "content": response})

# ── Clean up ─────────────────────────────────────────────────────────────

model.unload()
