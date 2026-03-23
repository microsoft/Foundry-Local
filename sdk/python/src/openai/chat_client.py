# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------

from __future__ import annotations

import logging
import json
import queue
import threading

from ..detail.core_interop import CoreInterop, InteropRequest
from ..exception import FoundryLocalException
from openai.types.chat.chat_completion_message_param import ChatCompletionMessageParam
from openai.types.chat.completion_create_params import CompletionCreateParamsBase, \
                                                       CompletionCreateParamsNonStreaming, \
                                                       CompletionCreateParamsStreaming
from openai.types.chat import ChatCompletion
from openai.types.chat.chat_completion_chunk import ChatCompletionChunk
from typing import Any, Callable, Dict, Generator, List, Optional, Union

logger = logging.getLogger(__name__)


class ChatClientSettings:
    """Settings for chat completion requests.

    Attributes match the OpenAI chat completion API parameters.
    Foundry-specific settings (``top_k``, ``random_seed``) are sent via metadata.
    """

    def __init__(
        self,
        frequency_penalty: Optional[float] = None,
        max_tokens: Optional[int] = None,
        n: Optional[int] = None,
        temperature: Optional[float] = None,
        presence_penalty: Optional[float] = None,
        random_seed: Optional[int] = None,
        top_k: Optional[int] = None,
        top_p: Optional[float] = None,
        response_format: Optional[Dict[str, Any]] = None,
        tool_choice: Optional[Dict[str, Any]] = None,
    ):
        self.frequency_penalty = frequency_penalty
        self.max_tokens = max_tokens
        self.n = n
        self.temperature = temperature
        self.presence_penalty = presence_penalty
        self.random_seed = random_seed
        self.top_k = top_k
        self.top_p = top_p
        self.response_format = response_format
        self.tool_choice = tool_choice

    def _serialize(self) -> Dict[str, Any]:
        """Serialize settings into an OpenAI-compatible request dict."""
        self._validate_response_format(self.response_format)
        self._validate_tool_choice(self.tool_choice)

        result: Dict[str, Any] = {
            k: v for k, v in {
                "frequency_penalty": self.frequency_penalty,
                "max_tokens": self.max_tokens,
                "n": self.n,
                "presence_penalty": self.presence_penalty,
                "temperature": self.temperature,
                "top_p": self.top_p,
                "response_format": self.response_format,
                "tool_choice": self.tool_choice,
            }.items() if v is not None
        }

        metadata: Dict[str, str] = {}
        if self.top_k is not None:
            metadata["top_k"] = str(self.top_k)
        if self.random_seed is not None:
            metadata["random_seed"] = str(self.random_seed)

        if metadata:
            result["metadata"] = metadata

        return result

    def _validate_response_format(self, response_format: Optional[Dict[str, Any]]) -> None:
        if response_format is None:
            return
        valid_types = ["text", "json_object", "json_schema", "lark_grammar"]
        fmt_type = response_format.get("type")
        if fmt_type not in valid_types:
            raise ValueError(f"ResponseFormat type must be one of: {', '.join(valid_types)}")
        grammar_types = ["json_schema", "lark_grammar"]
        if fmt_type in grammar_types:
            if fmt_type == "json_schema" and (
                not isinstance(response_format.get("json_schema"), str)
                or not response_format["json_schema"].strip()
            ):
                raise ValueError('ResponseFormat with type "json_schema" must have a valid json_schema string.')
            if fmt_type == "lark_grammar" and (
                not isinstance(response_format.get("lark_grammar"), str)
                or not response_format["lark_grammar"].strip()
            ):
                raise ValueError('ResponseFormat with type "lark_grammar" must have a valid lark_grammar string.')
        elif response_format.get("json_schema") or response_format.get("lark_grammar"):
            raise ValueError(
                f'ResponseFormat with type "{fmt_type}" should not have json_schema or lark_grammar properties.'
            )

    def _validate_tool_choice(self, tool_choice: Optional[Dict[str, Any]]) -> None:
        if tool_choice is None:
            return
        valid_types = ["none", "auto", "required", "function"]
        choice_type = tool_choice.get("type")
        if choice_type not in valid_types:
            raise ValueError(f"ToolChoice type must be one of: {', '.join(valid_types)}")
        if choice_type == "function" and (
            not isinstance(tool_choice.get("name"), str) or not tool_choice.get("name", "").strip()
        ):
            raise ValueError('ToolChoice with type "function" must have a valid name string.')
        elif choice_type != "function" and tool_choice.get("name"):
            raise ValueError(f'ToolChoice with type "{choice_type}" should not have a name property.')

class ChatClient:
    """OpenAI-compatible chat completions client backed by Foundry Local Core.

    Supports non-streaming and streaming completions with optional tool calling.

    Attributes:
        model_id: The ID of the loaded model variant.
        settings: Tunable ``ChatClientSettings`` (temperature, max tokens, etc.).
    """

    def __init__(self, model_id: str, core_interop: CoreInterop):
        self.model_id = model_id
        self.settings = ChatClientSettings()
        self._core_interop = core_interop

    def _validate_messages(self, messages: List[ChatCompletionMessageParam]) -> None:
        """Validate the messages list before sending to the native layer."""
        if not messages:
            raise ValueError("messages must be a non-empty list.")
        for i, msg in enumerate(messages):
            if not isinstance(msg, dict):
                raise ValueError(f"messages[{i}] must be a dict, got {type(msg).__name__}.")
            if "role" not in msg:
                raise ValueError(f"messages[{i}] is missing required key 'role'.")
            if "content" not in msg:
                raise ValueError(f"messages[{i}] is missing required key 'content'.")

    def _validate_tools(self, tools: Optional[List[Dict[str, Any]]]) -> None:
        """Validate the tools list before sending to the native layer."""
        if not tools:
            return
        if not isinstance(tools, list):
            raise ValueError("tools must be a list if provided.")
        for i, tool in enumerate(tools):
            if not isinstance(tool, dict) or not tool:
                raise ValueError(
                    f"tools[{i}] must be a non-null object with a valid 'type' and 'function' definition."
                )
            if not isinstance(tool.get("type"), str) or not tool["type"].strip():
                raise ValueError(f"tools[{i}] must have a 'type' property that is a non-empty string.")
            fn = tool.get("function")
            if not isinstance(fn, dict):
                raise ValueError(f"tools[{i}] must have a 'function' property that is a non-empty object.")
            if not isinstance(fn.get("name"), str) or not fn["name"].strip():
                raise ValueError(
                    f"tools[{i}]'s function must have a 'name' property that is a non-empty string."
                )

    def _create_request(
        self,
        messages: List[ChatCompletionMessageParam],
        streaming: bool,
        tools: Optional[List[Dict[str, Any]]] = None,
    ) -> str:
        request: Dict[str, Any] = {
            "model": self.model_id,
            "messages": messages,
            **({
                "tools": tools} if tools else {}),
            **({
                "stream": True} if streaming else {}),
            **self.settings._serialize(),
        }

        if streaming:
            chat_request = CompletionCreateParamsStreaming(request)
        else:
            chat_request = CompletionCreateParamsNonStreaming(request)

        return json.dumps(chat_request)

    def complete_chat(self, messages: List[ChatCompletionMessageParam], tools: Optional[List[Dict[str, Any]]] = None):
        """Perform a non-streaming chat completion.

        Args:
            messages: Conversation history as a list of OpenAI message dicts.
            tools: Optional list of tool definitions for function calling.

        Returns:
            A ``ChatCompletion`` response.

        Raises:
            ValueError: If messages is None, empty, or contains malformed entries.
            FoundryLocalException: If the native command returns an error.
        """
        self._validate_messages(messages)
        self._validate_tools(tools)
        chat_request_json = self._create_request(messages, streaming=False, tools=tools)

        # Send the request to the chat API
        request = InteropRequest(params={"OpenAICreateRequest": chat_request_json})
        response = self._core_interop.execute_command("chat_completions", request)
        if response.error is not None:
            raise FoundryLocalException(f"Error during chat completion: {response.error}")

        completion = ChatCompletion.model_validate_json(response.data)

        return completion

    def _complete_streaming_chat_impl(
        self,
        messages: List[ChatCompletionMessageParam],
        tools: Optional[List[Dict[str, Any]]] = None,
        callback: Optional[Callable[[ChatCompletionChunk], None]] = None,
    ) -> Optional[Generator[ChatCompletionChunk, None, None]]:
        # Validate eagerly (outside the generator so errors raise before any iteration)
        self._validate_messages(messages)
        self._validate_tools(tools)
        chat_request_json = self._create_request(messages, streaming=True, tools=tools)

        def _generate() -> Generator[ChatCompletionChunk, None, None]:
            _SENTINEL = object()
            chunk_queue: queue.Queue = queue.Queue()
            errors: List[Exception] = []

            def _on_chunk(response_str: str) -> None:
                raw = json.loads(response_str)
                # Foundry Local returns tool call chunks with "message.tool_calls" instead
                # of the standard streaming "delta.tool_calls". Normalize to delta format
                # so ChatCompletionChunk parses correctly.
                for choice in raw.get("choices", []):
                    if "message" in choice and "delta" not in choice:
                        msg = choice.pop("message")
                        # ChoiceDeltaToolCall requires "index"; add if missing
                        for i, tc in enumerate(msg.get("tool_calls", [])):
                            tc.setdefault("index", i)
                        choice["delta"] = msg
                chunk_queue.put(ChatCompletionChunk.model_validate(raw))

            def _run() -> None:
                try:
                    resp = self._core_interop.execute_command_with_callback(
                        "chat_completions",
                        InteropRequest(params={"OpenAICreateRequest": chat_request_json}),
                        _on_chunk,
                    )
                    if resp.error is not None:
                        errors.append(FoundryLocalException(f"Error during streaming chat completion: {resp.error}"))
                except Exception as exc:
                    errors.append(exc)
                finally:
                    chunk_queue.put(_SENTINEL)

            threading.Thread(target=_run, daemon=True).start()
            while (item := chunk_queue.get()) is not _SENTINEL:
                yield item
            if errors:
                raise errors[0]

        if callback is not None:
            for chunk in _generate():
                callback(chunk)
            return None
        return _generate()

    def complete_streaming_chat(
        self,
        messages: List[ChatCompletionMessageParam],
        tools_or_callback: Optional[Union[List[Dict[str, Any]], Callable[[ChatCompletionChunk], None]]] = None,
        callback: Optional[Callable[[ChatCompletionChunk], None]] = None,
    ) -> Optional[Generator[ChatCompletionChunk, None, None]]:
        """Perform a streaming chat completion.

        **Iterator mode** (no callback) — returns a generator; consume with ``for``:

            for chunk in client.complete_streaming_chat(messages):
                if chunk.choices[0].delta.content:
                    print(chunk.choices[0].delta.content, end="", flush=True)

        **Callback mode** — drives the stream internally, calls *callback* per chunk,
        returns ``None``:

            client.complete_streaming_chat(messages, on_chunk)
            client.complete_streaming_chat(messages, tools, on_chunk)

        Raises:
            ValueError: If messages or tools are malformed.
            TypeError: If a non-callable is passed as the callback.
            FoundryLocalException: If the native layer returns an error.
        """
        # Python has no true method overloading, so the second positional argument
        # serves a dual purpose — it can be either a list of tools or a callback
        # function. This lets callers use any of the four natural signatures:
        #
        #   complete_streaming_chat(messages)                   → iterator, no tools
        #   complete_streaming_chat(messages, tools)            → iterator, with tools
        #   complete_streaming_chat(messages, callback)         → callback, no tools
        #   complete_streaming_chat(messages, tools, callback)  → callback, with tools
        #
        # The dedicated `callback` parameter only comes into play when tools occupy
        # the second slot; in all other cases `tools_or_callback` carries the callback.
        if callable(tools_or_callback):
            tools: Optional[List[Dict[str, Any]]] = None
            cb = tools_or_callback
        elif isinstance(tools_or_callback, list):
            tools = tools_or_callback
            cb = callback
        else:
            tools = None
            cb = callback
        if cb is not None and not callable(cb):
            raise TypeError("callback must be a callable.")
        return self._complete_streaming_chat_impl(messages, tools, cb)