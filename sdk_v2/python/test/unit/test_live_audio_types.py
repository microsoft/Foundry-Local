# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Unit tests for live audio transcription dataclasses."""
from __future__ import annotations

from foundry_local_sdk.openai.live_audio_types import (
    CoreErrorResponse,
    LiveAudioTranscriptionOptions,
    LiveAudioTranscriptionResponse,
    TranscriptionContentPart,
)


class TestLiveAudioTranscriptionResponse:
    def test_response_from_json_native_shape(self):
        resp = LiveAudioTranscriptionResponse.from_json(
            '{"id":"x","is_final":true,"text":"hello","start_time":0.0,"end_time":1.0}'
        )
        assert resp.id == "x"
        assert resp.is_final is True
        assert resp.start_time == 0.0
        assert resp.end_time == 1.0
        assert len(resp.content) == 1
        assert resp.content[0].text == "hello"
        assert resp.content[0].transcript == "hello"

    def test_response_from_json_partial_fields(self):
        # Missing optional fields default sensibly.
        resp = LiveAudioTranscriptionResponse.from_json('{"text":"hi"}')
        assert resp.id is None
        assert resp.is_final is True  # default
        assert resp.start_time is None
        assert resp.end_time is None
        assert resp.content[0].text == "hi"

    def test_response_default_construction(self):
        r = LiveAudioTranscriptionResponse()
        assert r.content == []
        assert r.is_final is True


class TestCoreErrorResponse:
    def test_try_parse_valid_json(self):
        err = CoreErrorResponse.try_parse(
            '{"code":"BUSY","message":"Try again","isTransient":true}'
        )
        assert err is not None
        assert err.code == "BUSY"
        assert err.message == "Try again"
        assert err.is_transient is True

    def test_try_parse_garbage_returns_none(self):
        assert CoreErrorResponse.try_parse("not json at all") is None

    def test_try_parse_non_object_returns_none(self):
        # Valid JSON but not an object — should not crash, just return None.
        assert CoreErrorResponse.try_parse("[1, 2, 3]") is None
        assert CoreErrorResponse.try_parse('"a string"') is None


class TestLiveAudioTranscriptionOptions:
    def test_defaults(self):
        opts = LiveAudioTranscriptionOptions()
        assert opts.sample_rate == 16000
        assert opts.channels == 1
        assert opts.bits_per_sample == 16
        assert opts.language is None
        assert opts.push_queue_capacity == 100

    def test_snapshot_is_independent(self):
        opts = LiveAudioTranscriptionOptions(language="en")
        snap = opts.snapshot()
        opts.language = "fr"
        assert snap.language == "en"


class TestTranscriptionContentPart:
    def test_defaults(self):
        p = TranscriptionContentPart()
        assert p.text == ""
        assert p.transcript == ""
