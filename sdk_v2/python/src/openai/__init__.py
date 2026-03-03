# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""OpenAI-compatible clients for chat completions and audio transcription."""

from src.openai.chat_client import ChatClient, ChatSettings
from src.openai.audio_client import AudioClient

__all__ = ["AudioClient", "ChatClient", "ChatSettings"]
