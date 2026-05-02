# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Unit tests for live audio transcription — mirrors C# LiveAudioTranscriptionTests.cs.

These tests cover:
- LiveAudioTranscriptionResponse.from_json deserialization
- LiveAudioTranscriptionOptions defaults and snapshot
- CoreErrorResponse.try_parse
- Session state guards (append/get_transcription_stream before start)
"""

from __future__ import annotations

import json
import threading
import time
from unittest.mock import MagicMock

import pytest

from foundry_local_sdk.openai.live_audio_transcription_types import (
    CoreErrorResponse,
    LiveAudioTranscriptionOptions,
    LiveAudioTranscriptionResponse,
    TranscriptionContentPart,
)
from foundry_local_sdk.openai.live_audio_transcription_client import (
    LiveAudioTranscriptionSession,
)
from foundry_local_sdk.detail.core_interop import CoreInterop, Response
from foundry_local_sdk.exception import FoundryLocalException


# ---------------------------------------------------------------------------
# LiveAudioTranscriptionResponse.from_json tests
# ---------------------------------------------------------------------------


class TestFromJson:
    """LiveAudioTranscriptionResponse.from_json deserialization tests."""

    def test_parses_text_and_is_final(self):
        json_str = '{"is_final":true,"text":"hello world","start_time":null,"end_time":null}'

        result = LiveAudioTranscriptionResponse.from_json(json_str)

        assert result.content is not None
        assert len(result.content) == 1
        assert result.content[0].text == "hello world"
        assert result.content[0].transcript == "hello world"
        assert result.is_final is True

    def test_maps_timing_fields(self):
        json_str = '{"is_final":false,"text":"partial","start_time":1.5,"end_time":3.0}'

        result = LiveAudioTranscriptionResponse.from_json(json_str)

        assert result.content[0].text == "partial"
        assert result.is_final is False
        assert result.start_time == 1.5
        assert result.end_time == 3.0

    def test_empty_text_parses_successfully(self):
        json_str = '{"is_final":true,"text":"","start_time":null,"end_time":null}'

        result = LiveAudioTranscriptionResponse.from_json(json_str)

        assert result.content[0].text == ""
        assert result.is_final is True

    def test_only_start_time_sets_start_time(self):
        json_str = '{"is_final":true,"text":"word","start_time":2.0,"end_time":null}'

        result = LiveAudioTranscriptionResponse.from_json(json_str)

        assert result.start_time == 2.0
        assert result.end_time is None
        assert result.content[0].text == "word"

    def test_invalid_json_throws(self):
        with pytest.raises(Exception):
            LiveAudioTranscriptionResponse.from_json("not valid json")

    def test_content_has_text_and_transcript(self):
        json_str = '{"is_final":true,"text":"test","start_time":null,"end_time":null}'

        result = LiveAudioTranscriptionResponse.from_json(json_str)

        # Both Text and Transcript should have the same value
        assert result.content[0].text == "test"
        assert result.content[0].transcript == "test"

    def test_missing_fields_use_defaults(self):
        json_str = '{}'

        result = LiveAudioTranscriptionResponse.from_json(json_str)

        assert result.content[0].text == ""
        assert result.is_final is True
        assert result.start_time is None
        assert result.end_time is None


# ---------------------------------------------------------------------------
# LiveAudioTranscriptionOptions tests
# ---------------------------------------------------------------------------


class TestOptions:
    """LiveAudioTranscriptionOptions tests."""

    def test_default_values(self):
        options = LiveAudioTranscriptionOptions()

        assert options.sample_rate == 16000
        assert options.channels == 1
        assert options.bits_per_sample == 16
        assert options.language is None
        assert options.push_queue_capacity == 100

    def test_snapshot_creates_independent_copy(self):
        options = LiveAudioTranscriptionOptions(language="en")
        snapshot = options.snapshot()

        # Modify original — snapshot should be unaffected
        options.language = "zh"
        options.sample_rate = 44100

        assert snapshot.language == "en"
        assert snapshot.sample_rate == 16000


# ---------------------------------------------------------------------------
# CoreErrorResponse tests
# ---------------------------------------------------------------------------


class TestCoreErrorResponse:
    """CoreErrorResponse.try_parse tests."""

    def test_try_parse_valid_json(self):
        json_str = '{"code":"ASR_SESSION_NOT_FOUND","message":"Session not found","isTransient":false}'

        error = CoreErrorResponse.try_parse(json_str)

        assert error is not None
        assert error.code == "ASR_SESSION_NOT_FOUND"
        assert error.message == "Session not found"
        assert error.is_transient is False

    def test_try_parse_invalid_json_returns_none(self):
        result = CoreErrorResponse.try_parse("not json")
        assert result is None

    def test_try_parse_transient_error(self):
        json_str = '{"code":"BUSY","message":"Model busy","isTransient":true}'

        error = CoreErrorResponse.try_parse(json_str)

        assert error is not None
        assert error.is_transient is True


# ---------------------------------------------------------------------------
# Session state guard tests
# ---------------------------------------------------------------------------


class TestSessionStateGuards:
    """Verify that append/get_transcription_stream raise before start."""

    def _make_session(self) -> LiveAudioTranscriptionSession:
        """Create a session with a mock CoreInterop (no native DLLs needed)."""
        mock_interop = MagicMock(spec=CoreInterop)
        return LiveAudioTranscriptionSession("test-model", mock_interop)

    def test_append_before_start_throws(self):
        session = self._make_session()
        data = b'\x00' * 100

        with pytest.raises(FoundryLocalException):
            session.append(data)

    def test_get_transcription_stream_before_start_throws(self):
        session = self._make_session()

        with pytest.raises(FoundryLocalException):
            # Attempt to iterate — should raise immediately
            next(iter(session.get_transcription_stream()))

    def test_start_sets_started_flag(self):
        session = self._make_session()
        session._core_interop.start_audio_stream.return_value = Response(
            data="handle-123", error=None
        )
        session._core_interop.stop_audio_stream.return_value = Response(
            data=None, error=None
        )

        session.start()

        assert session._started is True
        assert session._session_handle == "handle-123"

        # Cleanup via public API
        session.stop()

    def test_double_start_throws(self):
        session = self._make_session()
        session._core_interop.start_audio_stream.return_value = Response(
            data="handle-123", error=None
        )
        session._core_interop.stop_audio_stream.return_value = Response(
            data=None, error=None
        )

        session.start()

        with pytest.raises(FoundryLocalException, match="already started"):
            session.start()

        # Cleanup via public API
        session.stop()

    def test_start_error_raises(self):
        session = self._make_session()
        session._core_interop.start_audio_stream.return_value = Response(
            data=None, error="init failed"
        )

        with pytest.raises(FoundryLocalException, match="Error starting"):
            session.start()

    def test_stop_without_start_is_noop(self):
        session = self._make_session()
        # Should not raise
        session.stop()

    # --- Cancellation tests ---

    def test_start_with_pre_set_session_cancel_event_raises(self):
        """Session-level cancel_event set before start() prevents native call."""
        cancel = threading.Event()
        cancel.set()

        mock_interop = MagicMock(spec=CoreInterop)
        session = LiveAudioTranscriptionSession("test-model", mock_interop, cancel)

        with pytest.raises(FoundryLocalException, match="cancelled"):
            session.start()

        # Native start must NOT have been invoked.
        mock_interop.start_audio_stream.assert_not_called()

    def test_session_level_cancel_unblocks_append_under_backpressure(self):
        """Setting the session-level cancel_event unblocks a backpressured append()."""
        cancel = threading.Event()
        mock_interop = MagicMock(spec=CoreInterop)
        mock_interop.start_audio_stream.return_value = Response(data="handle-1", error=None)
        # Keep push_audio_data slow so the queue fills.
        push_event = threading.Event()
        push_in_progress = threading.Event()

        def slow_push(*_args, **_kwargs):
            push_in_progress.set()
            push_event.wait()
            return Response(data=None, error=None)

        mock_interop.push_audio_data.side_effect = slow_push
        mock_interop.stop_audio_stream.return_value = Response(data=None, error=None)

        session = LiveAudioTranscriptionSession("test-model", mock_interop, cancel)
        session.settings.push_queue_capacity = 1  # tiny so we can fill quickly
        session.start()

        try:
            # First chunk: push thread takes it and blocks in slow_push.
            session.append(b"\x00" * 100)
            assert push_in_progress.wait(timeout=2.0), "push thread should pick up the first chunk"

            # Second chunk: fills the queue (capacity=1, push thread still busy).
            session.append(b"\x00" * 100)

            blocked_result = {"err": None, "completed": False}

            def blocked_append():
                try:
                    session.append(b"\x00" * 100)
                    blocked_result["completed"] = True
                except FoundryLocalException as e:
                    blocked_result["err"] = e

            t = threading.Thread(target=blocked_append, daemon=True)
            t.start()

            # Give the thread time to actually start blocking on the queue.
            t.join(timeout=0.3)
            assert t.is_alive(), "third append() should be blocked on queue capacity"

            # Trigger session-level cancel — should unblock append() with an exception.
            cancel.set()
            t.join(timeout=2.0)

            assert not t.is_alive(), "append() should have unblocked after cancel"
            assert blocked_result["err"] is not None, "append() should raise on cancel"
            assert "cancelled" in str(blocked_result["err"]).lower()
            assert blocked_result["completed"] is False, "chunk must NOT have been enqueued"
        finally:
            push_event.set()  # unblock the slow_push so stop() can drain
            session.stop()

    def test_get_transcription_stream_returns_cleanly_on_cancel(self):
        """Cancel event ends the generator without raising."""
        cancel = threading.Event()
        mock_interop = MagicMock(spec=CoreInterop)
        mock_interop.start_audio_stream.return_value = Response(data="handle-1", error=None)
        mock_interop.stop_audio_stream.return_value = Response(data=None, error=None)

        session = LiveAudioTranscriptionSession("test-model", mock_interop, cancel)
        session.start()

        try:
            results = []

            def consume():
                for r in session.get_transcription_stream():
                    results.append(r)

            t = threading.Thread(target=consume, daemon=True)
            t.start()
            time.sleep(0.2)  # let it start blocking on the empty queue
            assert t.is_alive(), "consumer should be blocked on empty queue"

            cancel.set()
            t.join(timeout=2.0)
            assert not t.is_alive(), "consumer should have returned cleanly on cancel"
            assert results == []  # no results were ever produced
        finally:
            session.stop()


# ---------------------------------------------------------------------------
# Session streaming integration test (mocked native core)
# ---------------------------------------------------------------------------


class TestSessionStreaming:
    """Verify the full push → output pipeline with a mocked native core."""

    def test_push_and_receive_transcription(self):
        """Simulate pushing audio and receiving transcription results."""
        mock_interop = MagicMock(spec=CoreInterop)

        # start_audio_stream returns a handle
        mock_interop.start_audio_stream.return_value = Response(
            data="session-42", error=None
        )

        # push_audio_data returns a transcription result
        push_response = json.dumps({
            "is_final": True,
            "text": "hello world",
            "start_time": 0.0,
            "end_time": 1.5,
        })
        mock_interop.push_audio_data.return_value = Response(
            data=push_response, error=None
        )

        # stop_audio_stream returns empty (no final result)
        mock_interop.stop_audio_stream.return_value = Response(
            data=None, error=None
        )

        session = LiveAudioTranscriptionSession("test-model", mock_interop)
        session.start()

        # Start reading results in background (must start before stop)
        results = []

        def read():
            for r in session.get_transcription_stream():
                results.append(r)

        reader = threading.Thread(target=read, daemon=True)
        reader.start()

        # Push a chunk of audio
        session.append(b'\x00' * 3200)

        # Stop to flush and complete
        session.stop()
        reader.join(timeout=5)

        assert len(results) == 1
        assert results[0].content[0].text == "hello world"
        assert results[0].is_final is True
        assert results[0].start_time == 0.0
        assert results[0].end_time == 1.5

    def test_push_error_surfaces_as_exception(self):
        """Verify that a native push error terminates the stream with an exception."""
        mock_interop = MagicMock(spec=CoreInterop)

        mock_interop.start_audio_stream.return_value = Response(
            data="session-42", error=None
        )
        mock_interop.push_audio_data.return_value = Response(
            data=None, error='{"code":"ASR_ERROR","message":"decode failed","isTransient":false}'
        )
        mock_interop.stop_audio_stream.return_value = Response(
            data=None, error=None
        )

        session = LiveAudioTranscriptionSession("test-model", mock_interop)
        session.start()

        try:
            session.append(b'\x00' * 3200)

            with pytest.raises(FoundryLocalException, match="Push failed"):
                for _ in session.get_transcription_stream():
                    pass
        finally:
            # Cleanup: stop to join the push thread even if assertions fail
            session.stop()

    def test_context_manager_calls_stop(self):
        """Verify context manager calls stop on exit."""
        mock_interop = MagicMock(spec=CoreInterop)
        mock_interop.start_audio_stream.return_value = Response(
            data="session-42", error=None
        )
        mock_interop.push_audio_data.return_value = Response(
            data=None, error=None
        )
        mock_interop.stop_audio_stream.return_value = Response(
            data=None, error=None
        )

        with LiveAudioTranscriptionSession("test-model", mock_interop) as session:
            session.start()

        # stop_audio_stream should have been called via context manager
        mock_interop.stop_audio_stream.assert_called_once()

    def test_stop_with_final_result(self):
        """Verify that stop() parses and surfaces a final transcription result."""
        mock_interop = MagicMock(spec=CoreInterop)
        mock_interop.start_audio_stream.return_value = Response(
            data="session-42", error=None
        )
        final_json = json.dumps({
            "is_final": True,
            "text": "final words",
            "start_time": 5.0,
            "end_time": 6.0,
        })
        mock_interop.stop_audio_stream.return_value = Response(
            data=final_json, error=None
        )

        session = LiveAudioTranscriptionSession("test-model", mock_interop)
        session.start()

        # Start reading results in background (must start before stop)
        results = []

        def read():
            for r in session.get_transcription_stream():
                results.append(r)

        reader = threading.Thread(target=read, daemon=True)
        reader.start()

        # No audio pushed — just stop to get final result
        session.stop()
        reader.join(timeout=5)

        assert len(results) == 1
        assert results[0].content[0].text == "final words"
