# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Tests for AudioClient – mirrors audioClient.test.ts."""

from __future__ import annotations

import pytest

from test.conftest import AUDIO_MODEL_ALIAS, get_git_repo_root

# Recording.mp3 lives at sdk_v2/testdata/Recording.mp3 relative to the repo root
AUDIO_FILE_PATH = str(get_git_repo_root() / "sdk_v2" / "testdata" / "Recording.mp3")
EXPECTED_TEXT = (
    " And lots of times you need to give people more than one link at a time."
    " You a band could give their fans a couple new videos from the live concert"
    " behind the scenes photo gallery and album to purchase like these next few links."
)


def _get_loaded_audio_model(catalog):
    """Helper: ensure the whisper model is selected, loaded, and return Model."""
    cached = catalog.get_cached_models()
    assert len(cached) > 0

    cached_variant = next((m for m in cached if m.alias == AUDIO_MODEL_ALIAS), None)
    assert cached_variant is not None, f"{AUDIO_MODEL_ALIAS} should be cached"

    model = catalog.get_model(AUDIO_MODEL_ALIAS)
    assert model is not None

    model.select_variant(cached_variant)
    model.load()
    return model


class TestAudioClient:
    """Audio Client Tests."""

    def test_should_transcribe_audio(self, catalog):
        """Non-streaming transcription of Recording.mp3."""
        model = _get_loaded_audio_model(catalog)
        try:
            audio_client = model.get_audio_client()
            assert audio_client is not None

            audio_client.settings.language = "en"
            audio_client.settings.temperature = 0.0

            response = audio_client.transcribe(AUDIO_FILE_PATH)

            assert response is not None
            assert hasattr(response, "text")
            assert isinstance(response.text, str)
            assert len(response.text) > 0
            assert response.text == EXPECTED_TEXT
        finally:
            model.unload()

    def test_should_transcribe_audio_with_temperature(self, catalog):
        """Non-streaming transcription with explicit temperature."""
        model = _get_loaded_audio_model(catalog)
        try:
            audio_client = model.get_audio_client()
            assert audio_client is not None

            audio_client.settings.language = "en"
            audio_client.settings.temperature = 0.0

            response = audio_client.transcribe(AUDIO_FILE_PATH)

            assert response is not None
            assert isinstance(response.text, str)
            assert len(response.text) > 0
            assert response.text == EXPECTED_TEXT
        finally:
            model.unload()

    def test_should_transcribe_audio_streaming(self, catalog):
        """Streaming transcription of Recording.mp3."""
        model = _get_loaded_audio_model(catalog)
        try:
            audio_client = model.get_audio_client()
            assert audio_client is not None

            audio_client.settings.language = "en"
            audio_client.settings.temperature = 0.0

            chunks = []

            def on_chunk(chunk):
                assert chunk is not None
                assert hasattr(chunk, "text")
                assert isinstance(chunk.text, str)
                assert len(chunk.text) > 0
                chunks.append(chunk.text)

            audio_client.transcribe_streaming(AUDIO_FILE_PATH, on_chunk)

            full_text = "".join(chunks)
            assert full_text == EXPECTED_TEXT
        finally:
            model.unload()

    def test_should_transcribe_audio_streaming_with_temperature(self, catalog):
        """Streaming transcription with explicit temperature."""
        model = _get_loaded_audio_model(catalog)
        try:
            audio_client = model.get_audio_client()
            assert audio_client is not None

            audio_client.settings.language = "en"
            audio_client.settings.temperature = 0.0

            chunks = []

            def on_chunk(chunk):
                assert chunk is not None
                assert isinstance(chunk.text, str)
                chunks.append(chunk.text)

            audio_client.transcribe_streaming(AUDIO_FILE_PATH, on_chunk)

            full_text = "".join(chunks)
            assert full_text == EXPECTED_TEXT
        finally:
            model.unload()

    def test_should_raise_for_empty_audio_file_path(self, catalog):
        """transcribe('') should raise."""
        model = catalog.get_model(AUDIO_MODEL_ALIAS)
        assert model is not None
        audio_client = model.get_audio_client()

        with pytest.raises(ValueError, match="Audio file path must be a non-empty string"):
            audio_client.transcribe("")

    def test_should_raise_for_streaming_empty_audio_file_path(self, catalog):
        """transcribe_streaming('') should raise."""
        model = catalog.get_model(AUDIO_MODEL_ALIAS)
        assert model is not None
        audio_client = model.get_audio_client()

        with pytest.raises(ValueError, match="Audio file path must be a non-empty string"):
            audio_client.transcribe_streaming("", lambda chunk: None)

    def test_should_raise_for_streaming_invalid_callback(self, catalog):
        """transcribe_streaming with invalid callback should raise."""
        model = catalog.get_model(AUDIO_MODEL_ALIAS)
        assert model is not None
        audio_client = model.get_audio_client()

        for invalid_callback in [None, 42, {}, "not a function"]:
            with pytest.raises(TypeError, match="Callback must be a valid function"):
                audio_client.transcribe_streaming(AUDIO_FILE_PATH, invalid_callback)
