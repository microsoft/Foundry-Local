# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Data types for live audio transcription streaming sessions."""
from __future__ import annotations

import json
from dataclasses import dataclass, field


@dataclass
class TranscriptionContentPart:
    """A content part within a live transcription response.

    Mirrors the OpenAI Realtime API ``ContentPart`` so that consumers can
    access the text via ``result.content[0].text`` or
    ``result.content[0].transcript``.

    Attributes:
        text: The transcribed text for this content part.
        transcript: Alias for ``text``.
    """

    text: str = ""
    transcript: str = ""


@dataclass
class LiveAudioTranscriptionResponse:
    """Transcription result for real-time audio streaming sessions.

    Shaped like the OpenAI Realtime API ``ConversationItem`` so the same consumer code keeps working when the
    transport moves to WebSocket.

    ``is_final`` reflects what the underlying model emits: ASR models with VAD-based finalization (e.g. Whisper
    variants) set it to ``False`` for interim hypotheses and ``True`` only at utterance boundaries. **Nemotron
    streams subword tokens and labels every one as final** — so a Nemotron-backed session yields one response per
    token, all with ``is_final=True``, and consumers must concatenate tokens themselves (without inserting
    separators — tokens already carry their own leading whitespace).

    Attributes:
        content: List of transcription content parts.
        is_final: Whether this is a final or partial (interim) result.
        start_time: Start time offset of this segment in the audio stream (seconds), if available.
        end_time: End time offset of this segment in the audio stream (seconds), if available.
        id: Unique identifier for this result, if available.
    """

    content: list[TranscriptionContentPart] = field(default_factory=list)
    is_final: bool = True
    start_time: float | None = None
    end_time: float | None = None
    id: str | None = None

    @classmethod
    def from_json(cls, json_str: str) -> LiveAudioTranscriptionResponse:
        """Deserialize a native Core JSON response.

        The native shape is a flat object (``id``, ``is_final``, ``text``,
        ``start_time``, ``end_time``); this method maps it into the
        ``ConversationItem``-shaped structure with a ``content`` list.

        Args:
            json_str: Raw JSON string from the native core.

        Returns:
            A ``LiveAudioTranscriptionResponse`` instance.

        Raises:
            json.JSONDecodeError: If *json_str* is not valid JSON.
        """
        raw = json.loads(json_str)
        text = raw.get("text", "") or ""
        return cls(
            content=[TranscriptionContentPart(text=text, transcript=text)],
            is_final=bool(raw.get("is_final", True)),
            start_time=raw.get("start_time"),
            end_time=raw.get("end_time"),
            id=raw.get("id"),
        )


@dataclass
class LiveAudioTranscriptionOptions:
    """Audio format settings for a live transcription streaming session.

    Snapshot-copied at ``LiveAudioTranscriptionSession.start()`` — mutating
    these fields after start has no effect on the running session.

    Attributes:
        sample_rate: PCM sample rate in Hz. Default: 16000.
        channels: Number of audio channels. Default: 1 (mono).
        bits_per_sample: Number of bits per audio sample. Default: 16.
        language: Optional BCP-47 language hint (e.g. ``"en"``, ``"zh"``).
        push_queue_capacity: Retained for backwards compatibility only.
            Not currently used — the native input queue is unbounded.
    """

    sample_rate: int = 16000
    channels: int = 1
    bits_per_sample: int = 16
    language: str | None = None
    # Retained for backwards compatibility; not currently consumed by the native session
    # (the native input queue is unbounded).
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

    @classmethod
    def try_parse(cls, error_string: str) -> CoreErrorResponse | None:
        """Attempt to parse a native error string as structured JSON.

        Returns ``None`` if the input is not valid JSON or does not match
        the expected object schema — callers should treat that as a
        permanent/unknown error.
        """
        try:
            raw = json.loads(error_string)
        except Exception:
            return None
        if not isinstance(raw, dict):
            return None
        return cls(
            code=str(raw.get("code", "") or ""),
            message=str(raw.get("message", "") or ""),
            is_transient=bool(raw.get("isTransient", False)),
        )
