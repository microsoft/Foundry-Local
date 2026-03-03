# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------

from __future__ import annotations

import logging
import json

from src.detail.core_interop import CoreInterop, InteropRequest
from openai.types.chat.chat_completion_message_param import *
from openai.types.chat.completion_create_params import CompletionCreateParamsBase, \
                                                       CompletionCreateParamsNonStreaming, \
                                                       CompletionCreateParamsStreaming
from openai.types.shared_params import Metadata
from openai.types.chat import ChatCompletion
from openai.types.chat.chat_completion_chunk import ChatCompletionChunk
from typing import Callable, List, Optional

logger = logging.getLogger(__name__)


class ChatSettings:
    """Settings supported by Foundry Local"""
    def __init__(
        self,
        frequency_penalty: Optional[float] = None,
        max_completion_tokens: Optional[int] = None,
        n: Optional[int] = None,
        temperature: Optional[float] = None,
        presence_penalty: Optional[float] = None,
        random_seed: Optional[int] = None,
        top_k: Optional[int] = None,
        top_p: Optional[float] = None
    ):
        self.frequency_penalty = frequency_penalty
        self.max_completion_tokens = max_completion_tokens
        self.n = n
        self.temperature = temperature
        self.presence_penalty = presence_penalty
        self.random_seed = random_seed
        self.top_k = top_k
        self.top_p = top_p

class ChatClient:
    """OpenAI-compatible chat completions client backed by Foundry Local Core.

    Supports non-streaming and streaming completions with optional tool calling.

    Attributes:
        model_id: The ID of the loaded model variant.
        settings: Tunable ``ChatSettings`` (temperature, max tokens, etc.).
    """

    def __init__(self, model_id: str, core_interop: CoreInterop):
        self.model_id = model_id
        self.settings = ChatSettings()
        self._core_interop = core_interop

    def _apply_settings(self, chat_request: CompletionCreateParamsBase):
        if self.settings.frequency_penalty is not None:
            chat_request["frequency_penalty"] = self.settings.frequency_penalty
        if self.settings.max_completion_tokens is not None:
            chat_request["max_completion_tokens"] = self.settings.max_completion_tokens
        if self.settings.n is not None:
            chat_request["n"] = self.settings.n
        if self.settings.temperature is not None:
            chat_request["temperature"] = self.settings.temperature
        if self.settings.presence_penalty is not None:
            chat_request["presence_penalty"] = self.settings.presence_penalty
        if self.settings.top_p is not None:
            chat_request["top_p"] = self.settings.top_p

        if self.settings.top_k is not None:
            chat_request["metadata"]["top_k"] = self.settings.top_k
        if self.settings.random_seed is not None:
            chat_request["metadata"]["random_seed"] = self.settings.random_seed

        chat_request["metadata"]["test"] = "value"

    def _create_request(self, messages: List[ChatCompletionMessageParam], streaming: bool) -> str:
        request = CompletionCreateParamsBase(
            {
                "model": self.model_id,
                "messages": messages,
                "metadata": {}
            }
        )

        self._apply_settings(request)

        if streaming:
            chat_request = CompletionCreateParamsStreaming(request)
        else:
            chat_request = CompletionCreateParamsNonStreaming(request)

        chat_request_json = json.dumps(chat_request)

        return chat_request_json

    def complete_chat(self, messages: List[ChatCompletionMessageParam]):
        """Perform a non-streaming chat completion.

        Args:
            messages: Conversation history as a list of OpenAI message dicts.

        Returns:
            A ``ChatCompletion`` response.

        Raises:
            ValueError: If the native command returns an error.
        """
        chat_request_json = self._create_request(messages, streaming=False)

        # Send the request to the chat API
        request = InteropRequest(params={"OpenAICreateRequest": chat_request_json})
        response = self._core_interop.execute_command("chat_completions", request)
        if response.error is not None:
            raise ValueError(f"Error during chat completion: {response.error}")

        completion = ChatCompletion.model_validate_json(response.data)

        return completion

    def complete_streaming_chat(self, messages: List[ChatCompletionMessageParam],
                                user_callback: Callable[[ChatCompletionChunk], None]):
        """Perform a streaming chat completion.

        Each incremental ``ChatCompletionChunk`` is passed to *user_callback*.

        Args:
            messages: Conversation history.
            user_callback: Called with each streaming chunk.

        Raises:
            ValueError: If the native command returns an error.
        """
        chat_request_json = self._create_request(messages, streaming=True)

        def callback_handler(response_str: str):
            completion = ChatCompletionChunk.model_validate_json(response_str)
            user_callback(completion)

        request = InteropRequest(params={"OpenAICreateRequest": chat_request_json})
        response = self._core_interop.execute_command_with_callback("chat_completions", request, callback_handler)
        if response.error is not None:
            raise ValueError(f"Error during streaming chat completion: {response.error}")