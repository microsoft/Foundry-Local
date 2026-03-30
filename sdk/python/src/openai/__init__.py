# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""OpenAI-compatible clients for chat completions and audio transcription."""

from .chat_client import ChatClient, ChatClientSettings
from .audio_client import AudioClient

__all__ = ["AudioClient", "ChatClient", "ChatClientSettings"]
