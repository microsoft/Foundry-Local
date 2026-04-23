# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""OpenAI-compatible clients for chat completions, audio, embeddings, and Responses API."""

from .chat_client import ChatClient, ChatClientSettings
from .audio_client import AudioClient
from .embedding_client import EmbeddingClient
from .responses_client import ResponsesClient, ResponsesClientSettings, ResponsesAPIError

__all__ = [
    "AudioClient",
    "ChatClient",
    "ChatClientSettings",
    "EmbeddingClient",
    "ResponsesAPIError",
    "ResponsesClient",
    "ResponsesClientSettings",
]
