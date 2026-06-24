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


async def test_transcribe_returns_response(audio_client):
    if not _RECORDING_PATH.is_file():
        pytest.skip(f"testdata/Recording.wav not found at {_RECORDING_PATH}")

    result = await audio_client.transcribe(str(_RECORDING_PATH))
    assert isinstance(result, AudioTranscriptionResponse)
    assert isinstance(result.text, str)
    assert result.text.strip() != ""


async def test_transcribe_empty_path_raises(audio_client):
    with pytest.raises(ValueError):
        await audio_client.transcribe("")


async def test_transcribe_nonexistent_path_raises(audio_client):
    # The client may validate at the Python layer (ValueError) or surface the native missing-file error
    # (FoundryLocalException) — accept either.
    with pytest.raises((ValueError, FoundryLocalException)):
        await audio_client.transcribe("/no/such/file.wav")


# Streaming-transcription coverage — mirrors the v1 ``test_should_transcribe_audio_streaming``
# tests in sdk/python/test/openai/test_audio_client.py. The v2 API is a Generator of
# ``AudioTranscriptionResponse``; the concatenation of all chunk texts should be
# equivalent to the non-streaming ``transcribe`` result for the same fixture.


async def test_transcribe_streaming_yields_chunks(audio_client):
    if not _RECORDING_PATH.is_file():
        pytest.skip(f"testdata/Recording.wav not found at {_RECORDING_PATH}")

    chunks: list[AudioTranscriptionResponse] = []
    async for chunk in audio_client.transcribe_streaming(str(_RECORDING_PATH)):
        assert isinstance(chunk, AudioTranscriptionResponse)
        assert isinstance(chunk.text, str)
        chunks.append(chunk)

    assert chunks, "streaming transcription must yield at least one chunk"

    full_text = "".join(c.text for c in chunks).strip()
    assert full_text != "", "concatenated streamed text must be non-empty"


async def test_transcribe_streaming_matches_non_streaming(audio_client):
    """Streaming concatenation should match (or be a close superset of) the one-shot result."""
    if not _RECORDING_PATH.is_file():
        pytest.skip(f"testdata/Recording.wav not found at {_RECORDING_PATH}")

    one_shot = (await audio_client.transcribe(str(_RECORDING_PATH))).text.strip()
    streamed_chunks = [c async for c in audio_client.transcribe_streaming(str(_RECORDING_PATH))]
    streamed = "".join(c.text for c in streamed_chunks).strip()

    assert one_shot, "one-shot transcription must be non-empty for comparison"
    assert streamed, "streamed transcription must be non-empty"

    # Whisper streaming chunks at segment boundaries; concatenation typically equals the
    # one-shot text, but exact equality is not guaranteed across whisper variants. Assert
    # a substantial overlap on word level rather than strict equality.
    one_shot_words = set(one_shot.lower().split())
    streamed_words = set(streamed.lower().split())
    overlap = one_shot_words & streamed_words
    assert len(overlap) >= max(1, len(one_shot_words) // 2), (
        f"streamed and one-shot transcriptions diverged.\n"
        f"  one-shot: {one_shot!r}\n"
        f"  streamed: {streamed!r}"
    )


async def test_transcribe_streaming_empty_path_raises(audio_client):
    # Validation runs when the async generator is first iterated.
    with pytest.raises(ValueError):
        async for _ in audio_client.transcribe_streaming(""):
            pass


async def test_transcribe_streaming_nonexistent_path_raises(audio_client):
    with pytest.raises((ValueError, FoundryLocalException)):
        async_gen = audio_client.transcribe_streaming("/no/such/file.wav")
        async for _ in async_gen:
            pass
