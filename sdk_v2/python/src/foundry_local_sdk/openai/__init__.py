# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""OpenAI-compatible client layer for Foundry Local.

Each client wraps a native model pointer and routes requests through the
low-level inference vtable using the OPENAI_JSON item type.  Clients are
stateless — a fresh native session is created per call.

Typical usage::

    model.load()
    client = model.get_chat_client()
    response = client.complete_chat([{"role": "user", "content": "Hello"}])
"""
from __future__ import annotations

# Check once here so every client in this package gets a clean, consistent
# error message instead of each file repeating its own try/except guard.
try:
    import openai as _openai  # noqa: F401
except ImportError:
    raise ImportError(
        "The third-party 'openai' package is required to use foundry_local_sdk.openai "
        "clients. Install it with: pip install openai"
    )

from .audio_client import AudioClient, AudioSettings, AudioTranscriptionResponse
from .chat_client import ChatClient, ChatClientSettings
from .embedding_client import EmbeddingClient
from .live_audio_session import LiveAudioTranscriptionSession
from .live_audio_types import (
    CoreErrorResponse,
    LiveAudioTranscriptionOptions,
    LiveAudioTranscriptionResponse,
    TranscriptionContentPart,
)

__all__ = [
    "AudioClient",
    "AudioSettings",
    "AudioTranscriptionResponse",
    "ChatClient",
    "ChatClientSettings",
    "CoreErrorResponse",
    "EmbeddingClient",
    "LiveAudioTranscriptionOptions",
    "LiveAudioTranscriptionResponse",
    "LiveAudioTranscriptionSession",
    "TranscriptionContentPart",
]
