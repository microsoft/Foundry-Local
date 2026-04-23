# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""OpenAI-compatible clients for chat completions and audio transcription."""

from .chat_client import ChatClient, ChatClientSettings
from .audio_client import AudioClient
from .responses_client import ResponsesClient, ResponsesClientSettings, get_output_text, create_image_content

__all__ = ["AudioClient", "ChatClient", "ChatClientSettings", "ResponsesClient", "ResponsesClientSettings", "get_output_text", "create_image_content"]
