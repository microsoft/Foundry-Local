# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Non-streaming ``AudioClient.transcribe`` integration tests.

Complements ``test_live_audio.py`` (which covers the streaming live-audio session) by exercising the one-shot
``transcribe()`` entry point against the bundled ``testdata/Recording.wav`` fixture.
"""
from __future__ import annotations

from pathlib import Path

import pytest

from foundry_local_sdk.exception import FoundryLocalException
from foundry_local_sdk.openai.audio_client import AudioTranscriptionResponse


# Raw s16le, 16 kHz, mono PCM — same file used by the streaming tests.
_RECORDING_PATH = Path(__file__).resolve().parents[3] / "testdata" / "Recording.wav"


@pytest.fixture
def audio_client(whisper_audio_model):
    """Function-scoped AudioClient — cheap to create, isolates per-test state.

    Uses :func:`whisper_audio_model` because the one-shot ``transcribe()`` path only works against whisper-family
    decoders today (see the fixture docstring).
    """
    return whisper_audio_model.get_audio_client()


def test_transcribe_returns_response(audio_client):
    if not _RECORDING_PATH.is_file():
        pytest.skip(f"testdata/Recording.wav not found at {_RECORDING_PATH}")

    result = audio_client.transcribe(str(_RECORDING_PATH))
    assert isinstance(result, AudioTranscriptionResponse)
    assert isinstance(result.text, str)
    assert result.text.strip() != ""


def test_transcribe_empty_path_raises(audio_client):
    with pytest.raises(ValueError):
        audio_client.transcribe("")


def test_transcribe_nonexistent_path_raises(audio_client):
    # The client may validate at the Python layer (ValueError) or surface the native missing-file error
    # (FoundryLocalException) — accept either.
    with pytest.raises((ValueError, FoundryLocalException)):
        audio_client.transcribe("/no/such/file.wav")
