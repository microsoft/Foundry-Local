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
    """

    sample_rate: int = 16000
    channels: int = 1
    bits_per_sample: int = 16
    language: str | None = None
    push_queue_capacity: int = 100

    def snapshot(self) -> LiveAudioTranscriptionOptions:
        return LiveAudioTranscriptionOptions(
            sample_rate=self.sample_rate,
            channels=self.channels,
            bits_per_sample=self.bits_per_sample,
            language=self.language,
            push_queue_capacity=self.push_queue_capacity,
        )


@dataclass
class CoreErrorResponse:
    """Structured error response from the native core."""

    code: str = ""
    message: str = ""
    is_transient: bool = False

    @classmethod
    def try_parse(cls, error_string: str) -> CoreErrorResponse | None:
        """Parse a native error string as JSON.  Returns ``None`` on any failure."""
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
