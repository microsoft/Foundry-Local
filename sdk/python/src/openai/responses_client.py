# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""OpenAI Responses API client — HTTP-only against the Foundry Local web service.

Unlike ``ChatClient`` / ``AudioClient`` which go through the native Core via FFI,
the Responses API is served exclusively by the embedded web service. The client
therefore uses ``requests`` for non-streaming calls and parses Server-Sent Events
inline for streaming.

Usage
-----
::

    manager.start_web_service()
    client = manager.create_responses_client("phi-4-mini")

    # Non-streaming
    resp = client.create("What is 2+2?")
    print(resp.output_text)

    # Streaming
    for event in client.create_streaming("Tell me a story"):
        if event.type == "response.output_text.delta":
            print(event.delta, end="", flush=True)
"""

from __future__ import annotations

import json
import logging
from dataclasses import is_dataclass
from typing import Any, Dict, Generator, List, Optional, Union
from urllib.parse import quote

import requests

from .responses_types import (
    DeleteResponseResult,
    InputItemsListResponse,
    ListResponsesResult,
    ReasoningConfig,
    ResponseObject,
    StreamingEvent,
    TextConfig,
    _parse_delete_result,
    _parse_input_items_list,
    _parse_list_responses,
    _parse_response_object,
    _to_dict,
    parse_streaming_event,
)

logger = logging.getLogger(__name__)

# Practical guard against misuse (e.g. passing a full response JSON by mistake).
# OpenAI does not publish a max ID length; 256 chars is conservative and generous.
_MAX_ID_LEN = 256


class ResponsesClientSettings:
    """Tunable settings applied to every Responses API request.

    Field names follow the OpenAI snake_case convention; serialization omits
    any ``None`` values so the server applies its own defaults.
    """

    def __init__(self) -> None:
        self.instructions: Optional[str] = None
        self.temperature: Optional[float] = None
        self.top_p: Optional[float] = None
        self.max_output_tokens: Optional[int] = None
        self.frequency_penalty: Optional[float] = None
        self.presence_penalty: Optional[float] = None
        self.tool_choice: Optional[Any] = None
        self.truncation: Optional[str] = None
        self.parallel_tool_calls: Optional[bool] = None
        self.store: Optional[bool] = True  # SDK default — matches OpenAI convention.
        self.metadata: Optional[Dict[str, str]] = None
        self.reasoning: Optional[ReasoningConfig] = None
        self.text: Optional[TextConfig] = None
        self.seed: Optional[int] = None
        # Transport settings — not sent to the API.
        self.timeout: float = 60.0
        """Seconds to wait for the server to connect and respond on non-streaming calls.
        For streaming, this is used only as the connection timeout; reads are unbounded."""

    def _serialize(self) -> Dict[str, Any]:
        raw: Dict[str, Any] = {
            "instructions": self.instructions,
            "temperature": self.temperature,
            "top_p": self.top_p,
            "max_output_tokens": self.max_output_tokens,
            "frequency_penalty": self.frequency_penalty,
            "presence_penalty": self.presence_penalty,
            "tool_choice": _to_dict(self.tool_choice) if is_dataclass(self.tool_choice) else self.tool_choice,
            "truncation": self.truncation,
            "parallel_tool_calls": self.parallel_tool_calls,
            "store": self.store,
            "metadata": self.metadata,
            "reasoning": _to_dict(self.reasoning) if self.reasoning is not None else None,
            "text": _to_dict(self.text) if self.text is not None else None,
            "seed": self.seed,
        }
        return {k: v for k, v in raw.items() if v is not None}


class ResponsesAPIError(Exception):
    """Raised for HTTP/transport errors against the Responses API."""

    def __init__(self, message: str, status_code: Optional[int] = None, body: Optional[str] = None):
        super().__init__(message)
        self.status_code = status_code
        self.body = body


class ResponsesClient:
    """Client for the OpenAI Responses API served by Foundry Local.

    Construct via ``manager.create_responses_client(model_id)`` or
    ``model.create_responses_client(base_url)``.
    """

    def __init__(self, base_url: str, model_id: Optional[str] = None):
        if not isinstance(base_url, str) or not base_url.strip():
            raise ValueError("base_url must be a non-empty string.")
        self._base_url = base_url.rstrip("/")
        self._model_id = model_id
        self.settings = ResponsesClientSettings()

    # ------------------------------------------------------------------ public

    def create(
        self,
        input: Union[str, List[Any]],
        **options: Any,
    ) -> ResponseObject:
        """Create a response (non-streaming)."""
        body = self._build_request(input, options, stream=False)
        raw = self._post_json("/v1/responses", body)
        return _parse_response_object(raw)

    def create_streaming(
        self,
        input: Union[str, List[Any]],
        **options: Any,
    ) -> Generator[StreamingEvent, None, None]:
        """Create a response with SSE streaming.

        Returns a generator yielding :class:`StreamingEvent` objects. The HTTP
        connection is closed automatically when the generator is exhausted or
        garbage-collected.
        """
        body = self._build_request(input, options, stream=True)
        return self._post_stream("/v1/responses", body)

    def get(self, response_id: str) -> ResponseObject:
        self._validate_id(response_id, "response_id")
        raw = self._request_json("GET", f"/v1/responses/{quote(response_id, safe='')}")
        return _parse_response_object(raw)

    def delete(self, response_id: str) -> DeleteResponseResult:
        self._validate_id(response_id, "response_id")
        raw = self._request_json("DELETE", f"/v1/responses/{quote(response_id, safe='')}")
        return _parse_delete_result(raw)

    def cancel(self, response_id: str) -> ResponseObject:
        self._validate_id(response_id, "response_id")
        raw = self._request_json("POST", f"/v1/responses/{quote(response_id, safe='')}/cancel")
        return _parse_response_object(raw)

    def get_input_items(self, response_id: str) -> InputItemsListResponse:
        self._validate_id(response_id, "response_id")
        raw = self._request_json("GET", f"/v1/responses/{quote(response_id, safe='')}/input_items")
        return _parse_input_items_list(raw)

    def list(self) -> ListResponsesResult:
        raw = self._request_json("GET", "/v1/responses")
        return _parse_list_responses(raw)

    # ---------------------------------------------------------------- internal

    def _build_request(
        self,
        input: Union[str, List[Any]],
        options: Dict[str, Any],
        stream: bool,
    ) -> Dict[str, Any]:
        self._validate_input(input)
        if options.get("tools") is not None:
            self._validate_tools(options["tools"])

        model = options.pop("model", None) or self._model_id
        if not isinstance(model, str) or not model.strip():
            raise ValueError(
                "Model must be specified via create_responses_client(model_id) or options['model']."
            )

        # Normalize input: convert dataclasses to dicts for the wire format.
        if isinstance(input, list):
            wire_input = [_to_dict(i) if is_dataclass(i) else i for i in input]
        else:
            wire_input = input

        # Normalize other dataclass-shaped options (tools, reasoning, etc.).
        normalized_options: Dict[str, Any] = {}
        for key, value in options.items():
            if value is None:
                continue
            if is_dataclass(value):
                normalized_options[key] = _to_dict(value)
            elif isinstance(value, list):
                normalized_options[key] = [_to_dict(v) if is_dataclass(v) else v for v in value]
            else:
                normalized_options[key] = value

        body: Dict[str, Any] = {"model": model, "input": wire_input}
        # Merge order: model+input → settings defaults → per-call overrides
        body.update(self.settings._serialize())
        body.update(normalized_options)
        if stream:
            body["stream"] = True
        return body

    @staticmethod
    def _validate_input(input: Any) -> None:
        if input is None:
            raise ValueError("Input cannot be None.")
        if isinstance(input, str):
            if not input.strip():
                raise ValueError("Input string cannot be empty.")
            return
        if isinstance(input, list):
            if len(input) == 0:
                raise ValueError("Input items list cannot be empty.")
            for i, item in enumerate(input):
                if is_dataclass(item):
                    t = getattr(item, "type", None)
                elif isinstance(item, dict):
                    t = item.get("type")
                else:
                    raise ValueError(f"input[{i}] must be a dict or dataclass.")
                if not isinstance(t, str) or not t.strip():
                    raise ValueError(f"input[{i}] must have a non-empty 'type' field.")
            return
        raise ValueError("Input must be a string or a list of input items.")

    @staticmethod
    def _validate_tools(tools: Any) -> None:
        if not isinstance(tools, list):
            raise ValueError("tools must be a list if provided.")
        for i, tool in enumerate(tools):
            if is_dataclass(tool):
                t = getattr(tool, "type", None)
                name = getattr(tool, "name", None)
            elif isinstance(tool, dict):
                t = tool.get("type")
                name = tool.get("name")
            else:
                raise ValueError(f"tools[{i}] must be a dict or FunctionToolDefinition.")
            if t != "function":
                raise ValueError(f"tools[{i}] must have type 'function'.")
            if not isinstance(name, str) or not name.strip():
                raise ValueError(f"tools[{i}] must have a non-empty 'name'.")

    @staticmethod
    def _validate_id(value: str, param: str) -> None:
        if not isinstance(value, str) or not value.strip():
            raise ValueError(f"{param} must be a non-empty string.")
        if len(value) > _MAX_ID_LEN:
            raise ValueError(f"{param} exceeds maximum length ({_MAX_ID_LEN}).")

    # ----- HTTP plumbing -----

    def _url(self, path: str) -> str:
        return f"{self._base_url}{path}"

    def _request_json(self, method: str, path: str, body: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        timeout = self.settings.timeout
        try:
            if body is not None:
                resp = requests.request(
                    method,
                    self._url(path),
                    headers={"Content-Type": "application/json", "Accept": "application/json"},
                    data=json.dumps(body),
                    timeout=timeout,
                )
            else:
                resp = requests.request(
                    method,
                    self._url(path),
                    headers={"Accept": "application/json"},
                    timeout=timeout,
                )
        except requests.RequestException as e:
            raise ResponsesAPIError(f"Network error calling {method} {path}: {e}") from e

        return self._handle_json_response(resp, method, path)

    def _post_json(self, path: str, body: Dict[str, Any]) -> Dict[str, Any]:
        return self._request_json("POST", path, body)

    @staticmethod
    def _handle_json_response(resp: requests.Response, method: str, path: str) -> Dict[str, Any]:
        text = resp.text
        if not resp.ok:
            raise ResponsesAPIError(
                f"Responses API error ({resp.status_code}) for {method} {path}: {text[:500]}",
                status_code=resp.status_code,
                body=text,
            )
        try:
            return json.loads(text) if text else {}
        except json.JSONDecodeError as e:
            raise ResponsesAPIError(
                f"Failed to parse response JSON from {method} {path}: {text[:200]}"
            ) from e

    def _post_stream(
        self, path: str, body: Dict[str, Any]
    ) -> Generator[StreamingEvent, None, None]:
        # Use (connect_timeout, None) so the connection attempt can time out but
        # the read side is unbounded — streaming responses can be arbitrarily long.
        connect_timeout = self.settings.timeout
        try:
            resp = requests.post(
                self._url(path),
                headers={"Content-Type": "application/json", "Accept": "text/event-stream"},
                data=json.dumps(body),
                stream=True,
                timeout=(connect_timeout, None),
            )
        except requests.RequestException as e:
            raise ResponsesAPIError(f"Network error calling POST {path}: {e}") from e

        if not resp.ok:
            body_text = resp.text
            resp.close()
            raise ResponsesAPIError(
                f"Responses API error ({resp.status_code}) for POST {path}: {body_text[:500]}",
                status_code=resp.status_code,
                body=body_text,
            )

        return _iter_sse_events(resp)


def _iter_sse_events(resp: requests.Response) -> Generator[StreamingEvent, None, None]:
    """Parse an SSE response into a stream of :class:`StreamingEvent` objects.

    Closes the underlying HTTP connection when the generator ends for any
    reason (completion, [DONE], exception, or GC).

    Uses a single string buffer and splits on double-newline boundaries to
    avoid the O(n) cost of joining a growing list on every chunk.
    """
    try:
        buffer = ""
        for chunk in resp.iter_content(chunk_size=None, decode_unicode=False):
            if not chunk:
                continue
            text = chunk.decode("utf-8", errors="replace") if isinstance(chunk, bytes) else chunk
            buffer += text.replace("\r\n", "\n")

            while "\n\n" in buffer:
                block, buffer = buffer.split("\n\n", 1)
                event = _parse_sse_block(block)
                if event is _SSE_DONE:
                    return
                if event is not None:
                    yield event

        # Flush any residual block not terminated by a blank line.
        tail = buffer.strip()
        if tail:
            event = _parse_sse_block(tail)
            if event is not None and event is not _SSE_DONE:
                yield event
    finally:
        resp.close()


_SSE_DONE = object()  # sentinel returned for the `data: [DONE]` terminator


def _parse_sse_block(block: str) -> Any:
    """Parse a single SSE block (already stripped of its trailing blank line)."""
    trimmed = block.strip()
    if not trimmed:
        return None
    if trimmed == "data: [DONE]":
        return _SSE_DONE

    data_lines: List[str] = []
    for line in trimmed.split("\n"):
        if line.startswith("data: "):
            data_lines.append(line[6:])
        elif line == "data:":
            data_lines.append("")
        # `event:`, `id:`, `retry:` fields are ignored — the type lives in the JSON payload.

    if not data_lines:
        return None

    data = "\n".join(data_lines)
    if data == "[DONE]":
        return _SSE_DONE
    try:
        parsed = json.loads(data)
    except json.JSONDecodeError as e:
        raise ResponsesAPIError(f"Failed to parse streaming event JSON: {e}") from e
    if not isinstance(parsed, dict):
        return None
    return parse_streaming_event(parsed)


__all__ = [
    "ResponsesClient",
    "ResponsesClientSettings",
    "ResponsesAPIError",
]
