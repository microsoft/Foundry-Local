# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Data types for live audio transcription streaming sessions."""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from typing import List, Optional


@dataclass
class TranscriptionContentPart:
    """A content part within a live transcription response.

    Mirrors the OpenAI Realtime API ``ContentPart`` structure so that
    ``result.content[0].text`` and ``result.content[0].transcript``
    both return the transcribed text.

    Attributes:
        text: The transcribed text for this content part.
        transcript: Alias for ``text`` (OpenAI Realtime API compatibility).
    """

    text: str = ""
    transcript: str = ""


@dataclass
class LiveAudioTranscriptionResponse:
    """Transcription result for real-time audio streaming sessions.

    Shaped like the OpenAI Realtime API ``ConversationItem`` so that
    consumers can access text via ``result.content[0].text`` or
    ``result.content[0].transcript``.

    Attributes:
        content: List of transcription content parts.
        is_final: Whether this is a final or partial (interim) result.
            Nemotron models always return ``True``.
        start_time: Start time offset of this segment in the audio stream (seconds).
        end_time: End time offset of this segment in the audio stream (seconds).
        id: Unique identifier for this result (if available).
    """

    content: List[TranscriptionContentPart] = field(default_factory=list)
    is_final: bool = True
    start_time: Optional[float] = None
    end_time: Optional[float] = None
    id: Optional[str] = None

    @staticmethod
    def from_json(json_str: str) -> LiveAudioTranscriptionResponse:
        """Deserialize a native Core JSON response into a ``LiveAudioTranscriptionResponse``.

        The native JSON format uses flat fields (``text``, ``is_final``,
        ``start_time``, ``end_time``).  This method maps them into the
        ``ConversationItem``-shaped structure with a ``content`` list.

        Args:
            json_str: Raw JSON string from the native core.

        Returns:
            A ``LiveAudioTranscriptionResponse`` instance.

        Raises:
            json.JSONDecodeError: If *json_str* is not valid JSON.
            Exception: If deserialization fails.
        """
        raw = json.loads(json_str)
        text = raw.get("text", "")
        return LiveAudioTranscriptionResponse(
            content=[TranscriptionContentPart(text=text, transcript=text)],
            is_final=raw.get("is_final", True),
            start_time=raw.get("start_time"),
            end_time=raw.get("end_time"),
        )


@dataclass
class LiveAudioTranscriptionOptions:
    """Audio format settings for a live transcription streaming session.

    Must be configured before calling :meth:`LiveAudioTranscriptionSession.start`.
    Settings are frozen (snapshot-copied) once the session starts.

    Attributes:
        sample_rate: PCM sample rate in Hz.  Default: 16000.
        channels: Number of audio channels.  Default: 1 (mono).
        bits_per_sample: Number of bits per audio sample.  Default: 16.
        language: Optional BCP-47 language hint (e.g. ``"en"``, ``"zh"``).
        push_queue_capacity: Maximum number of audio chunks buffered in the
            internal push queue.  Default: 100 (~3 s at typical chunk sizes).
    """

    sample_rate: int = 16000
    channels: int = 1
    bits_per_sample: int = 16
    language: Optional[str] = None
    push_queue_capacity: int = 100

    def snapshot(self) -> LiveAudioTranscriptionOptions:
        """Return a shallow copy of these settings (freeze pattern)."""
        return LiveAudioTranscriptionOptions(
            sample_rate=self.sample_rate,
            channels=self.channels,
            bits_per_sample=self.bits_per_sample,
            language=self.language,
            push_queue_capacity=self.push_queue_capacity,
        )


@dataclass
class CoreErrorResponse:
    """Structured error response from the native core.

    Attributes:
        code: Error code string (e.g. ``"ASR_SESSION_NOT_FOUND"``).
        message: Human-readable error description.
        is_transient: Whether the error is transient and may succeed on retry.
    """

    code: str = ""
    message: str = ""
    is_transient: bool = False

    @staticmethod
    def try_parse(error_string: str) -> Optional[CoreErrorResponse]:
        """Attempt to parse a native error string as structured JSON.

        Returns ``None`` if the error is not valid JSON or doesn't match
        the expected schema, which should be treated as a permanent/unknown error.
        """
        try:
            raw = json.loads(error_string)
            return CoreErrorResponse(
                code=raw.get("code", ""),
                message=raw.get("message", ""),
                is_transient=raw.get("isTransient", False),
            )
        except Exception:
            return None
