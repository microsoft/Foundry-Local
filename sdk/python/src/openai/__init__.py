# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""OpenAI-compatible clients for chat completions and audio transcription."""

from .chat_client import ChatClient, ChatClientSettings
from .audio_client import AudioClient
from .embedding_client import EmbeddingClient
from .live_audio_transcription_client import LiveAudioTranscriptionSession
from .live_audio_transcription_types import (
    CoreErrorResponse,
    LiveAudioTranscriptionOptions,
    LiveAudioTranscriptionResponse,
    TranscriptionContentPart,
)

__all__ = [
    "AudioClient",
    "ChatClient",
    "ChatClientSettings",
    "CoreErrorResponse",
    "EmbeddingClient",
    "LiveAudioTranscriptionOptions",
    "LiveAudioTranscriptionResponse",
    "LiveAudioTranscriptionSession",
    "TranscriptionContentPart",
]
