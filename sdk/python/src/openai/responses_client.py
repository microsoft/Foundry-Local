# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------

from __future__ import annotations

import base64
import io
import json
import logging
import os
import urllib.request
import urllib.error
from typing import Any, Callable, Dict, Generator, List, Optional, Union

from ..exception import FoundryLocalException

logger = logging.getLogger(__name__)

# ── Image helpers ──────────────────────────────────────────────────────────

_MIME_TYPES = {
    ".jpg": "image/jpeg", ".jpeg": "image/jpeg", ".png": "image/png",
    ".gif": "image/gif", ".webp": "image/webp", ".bmp": "image/bmp",
}

_DEFAULT_MAX_IMAGE_DIM = 480


def _get_mime_type(file_path: str) -> str:
    ext = os.path.splitext(file_path)[1].lower()
    return _MIME_TYPES.get(ext, "image/png")


def _resize_image_if_needed(
    image_bytes: bytes, mime: str, max_dim: int,
) -> tuple[bytes, str]:
    """Resize image if either dimension exceeds max_dim. Returns (bytes, mime)."""
    try:
        from PIL import Image
    except ImportError:
        return image_bytes, mime

    img = Image.open(io.BytesIO(image_bytes))
    w, h = img.size
    if w <= max_dim and h <= max_dim:
        return image_bytes, mime

    scale = min(max_dim / w, max_dim / h)
    nw, nh = int(w * scale), int(h * scale)
    logger.info("Resizing image %dx%d -> %dx%d", w, h, nw, nh)
    img = img.resize((nw, nh), Image.LANCZOS)
    buf = io.BytesIO()
    img.save(buf, format="PNG")
    return buf.getvalue(), "image/png"


def create_image_content(
    source: str,
    *,
    detail: str = "auto",
    max_dim: int = _DEFAULT_MAX_IMAGE_DIM,
) -> Dict[str, Any]:
    """Create an ``input_image`` content part from a file path or URL.

    Reads the image, optionally resizes it if either dimension exceeds *max_dim*,
    and returns a dict ready to be included in a message's ``content`` list.

    Resizing requires `Pillow <https://pypi.org/project/Pillow/>`_ to be installed.
    If Pillow is not available, the image is sent at its original size.

    Args:
        source: A local file path or an ``http://``/``https://`` URL.
        detail: Image detail level (``"auto"``, ``"low"``, or ``"high"``).
        max_dim: Maximum dimension (width or height) in pixels before resizing.
            Defaults to 480.

    Returns:
        A dict with keys ``type``, ``image_url``, ``media_type``, and ``detail``.

    Example::

        content = [
            create_image_content("photo.png"),
            {"type": "input_text", "text": "Describe this image"},
        ]
        input_items = [{"type": "message", "role": "user", "content": content}]
        response = client.create(input_items)
    """
    if source.startswith("http://") or source.startswith("https://"):
        req = urllib.request.Request(source, headers={"User-Agent": "FoundryLocal-PythonSdk/1.0"})
        with urllib.request.urlopen(req) as resp:
            mime = resp.headers.get("Content-Type", "image/jpeg").split(";")[0].strip()
            data = resp.read()
    else:
        abs_path = os.path.abspath(source)
        with open(abs_path, "rb") as f:
            data = f.read()
        mime = _get_mime_type(abs_path)

    data, mime = _resize_image_if_needed(data, mime, max_dim)
    b64 = base64.b64encode(data).decode("ascii")
    return {
        "type": "input_image",
        "image_url": f"data:{mime};base64,{b64}",
        "media_type": mime,
        "detail": detail,
    }


class ResponsesClientSettings:
    """Configuration settings for the Responses API client.

    Attributes match the OpenAI Responses API parameters.
    """

    def __init__(
        self,
        instructions: Optional[str] = None,
        temperature: Optional[float] = None,
        top_p: Optional[float] = None,
        max_output_tokens: Optional[int] = None,
        frequency_penalty: Optional[float] = None,
        presence_penalty: Optional[float] = None,
        tool_choice: Optional[Union[str, Dict[str, str]]] = None,
        truncation: Optional[str] = None,
        parallel_tool_calls: Optional[bool] = None,
        store: Optional[bool] = None,
        metadata: Optional[Dict[str, str]] = None,
        reasoning: Optional[Dict[str, str]] = None,
        text: Optional[Dict[str, Any]] = None,
        seed: Optional[int] = None,
    ):
        self.instructions = instructions
        self.temperature = temperature
        self.top_p = top_p
        self.max_output_tokens = max_output_tokens
        self.frequency_penalty = frequency_penalty
        self.presence_penalty = presence_penalty
        self.tool_choice = tool_choice
        self.truncation = truncation
        self.parallel_tool_calls = parallel_tool_calls
        self.store = store
        self.metadata = metadata
        self.reasoning = reasoning
        self.text = text
        self.seed = seed

    def _serialize(self) -> Dict[str, Any]:
        """Serialize settings into an OpenAI Responses API-compatible request dict."""
        result: Dict[str, Any] = {
            k: v for k, v in {
                "instructions": self.instructions,
                "temperature": self.temperature,
                "top_p": self.top_p,
                "max_output_tokens": self.max_output_tokens,
                "frequency_penalty": self.frequency_penalty,
                "presence_penalty": self.presence_penalty,
                "tool_choice": self.tool_choice,
                "truncation": self.truncation,
                "parallel_tool_calls": self.parallel_tool_calls,
                "store": self.store,
                "metadata": self.metadata,
                "reasoning": self.reasoning,
                "text": self.text,
                "seed": self.seed,
            }.items() if v is not None
        }
        return result


class ResponsesClient:
    """Client for the OpenAI Responses API served by Foundry Local's embedded web service.

    Unlike ChatClient/AudioClient (which use FFI via CoreInterop), the Responses API
    is HTTP-only. This client uses urllib for all operations and parses Server-Sent Events
    for streaming.

    Create via ``FoundryLocalManager.create_responses_client()`` or
    ``model.get_responses_client(base_url)``.

    Example::

        manager = FoundryLocalManager.instance
        manager.start_web_service()
        client = manager.create_responses_client('my-model-id')

        # Non-streaming
        response = client.create('Hello, world!')
        print(response['output'])

        # Streaming
        for event in client.create_streaming('Tell me a story'):
            if event['type'] == 'response.output_text.delta':
                print(event['delta'], end='', flush=True)
    """

    def __init__(self, base_url: str, model_id: Optional[str] = None):
        """
        Args:
            base_url: The base URL of the Foundry Local web service (e.g. ``http://127.0.0.1:5273``).
            model_id: Optional default model ID. Can be overridden per-request via options.
        """
        if not base_url or not isinstance(base_url, str) or not base_url.strip():
            raise ValueError("base_url must be a non-empty string.")
        self._base_url = base_url.rstrip("/")
        self._model_id = model_id
        self.settings = ResponsesClientSettings()

    def create(
        self,
        input: Union[str, List[Dict[str, Any]]],
        options: Optional[Dict[str, Any]] = None,
    ) -> Dict[str, Any]:
        """Creates a model response (non-streaming).

        Args:
            input: A string prompt or list of input items.
            options: Additional request parameters that override client settings.

        Returns:
            The completed Response object as a dict.
        """
        self._validate_input(input)
        if options and options.get("tools"):
            self._validate_tools(options["tools"])

        body = self._build_request(input, options, stream=False)
        return self._fetch_json("/v1/responses", method="POST", body=body)

    def create_streaming(
        self,
        input: Union[str, List[Dict[str, Any]]],
        options: Optional[Dict[str, Any]] = None,
    ) -> Generator[Dict[str, Any], None, None]:
        """Creates a model response with streaming via Server-Sent Events.

        Args:
            input: A string prompt or list of input items.
            options: Additional request parameters that override client settings.

        Yields:
            Streaming event dicts as they arrive.
        """
        self._validate_input(input)
        if options and options.get("tools"):
            self._validate_tools(options["tools"])

        body = self._build_request(input, options, stream=True)
        yield from self._parse_sse_stream("/v1/responses", body)

    def get(self, response_id: str) -> Dict[str, Any]:
        """Retrieves a stored response by ID.

        Args:
            response_id: The ID of the response to retrieve.

        Returns:
            The Response object as a dict.
        """
        self._validate_id(response_id, "response_id")
        return self._fetch_json(
            f"/v1/responses/{urllib.request.quote(response_id, safe='')}",
            method="GET",
        )

    def delete(self, response_id: str) -> Dict[str, Any]:
        """Deletes a stored response by ID.

        Args:
            response_id: The ID of the response to delete.

        Returns:
            The deletion result as a dict.
        """
        self._validate_id(response_id, "response_id")
        return self._fetch_json(
            f"/v1/responses/{urllib.request.quote(response_id, safe='')}",
            method="DELETE",
        )

    def cancel(self, response_id: str) -> Dict[str, Any]:
        """Cancels an in-progress response.

        Args:
            response_id: The ID of the response to cancel.

        Returns:
            The cancelled Response object as a dict.
        """
        self._validate_id(response_id, "response_id")
        return self._fetch_json(
            f"/v1/responses/{urllib.request.quote(response_id, safe='')}/cancel",
            method="POST",
        )

    def get_input_items(self, response_id: str) -> Dict[str, Any]:
        """Retrieves input items for a stored response.

        Args:
            response_id: The ID of the response.

        Returns:
            The list of input items as a dict.
        """
        self._validate_id(response_id, "response_id")
        return self._fetch_json(
            f"/v1/responses/{urllib.request.quote(response_id, safe='')}/input_items",
            method="GET",
        )

    # ========================================================================
    # Internal helpers
    # ========================================================================

    def _build_request(
        self,
        input: Union[str, List[Dict[str, Any]]],
        options: Optional[Dict[str, Any]],
        stream: bool,
    ) -> Dict[str, Any]:
        model = (options or {}).get("model") or self._model_id
        if not model or not isinstance(model, str) or not model.strip():
            raise ValueError(
                "Model must be specified either in the constructor, via create_responses_client(model_id), or in options['model']."
            )

        serialized = self.settings._serialize()
        request: Dict[str, Any] = {
            "model": model,
            "input": input,
            **serialized,
            **(options or {}),
            "stream": stream,
        }
        return request

    def _validate_input(self, input: Union[str, List[Dict[str, Any]]]) -> None:
        if input is None:
            raise ValueError("Input cannot be None.")
        if isinstance(input, str):
            if not input.strip():
                raise ValueError("Input string cannot be empty.")
            return
        if isinstance(input, list):
            if len(input) == 0:
                raise ValueError("Input items list cannot be empty.")
            for item in input:
                if not isinstance(item, dict):
                    raise ValueError("Each input item must be a dict.")
                if not isinstance(item.get("type"), str) or not item["type"].strip():
                    raise ValueError('Each input item must have a "type" property that is a non-empty string.')
            return
        raise ValueError("Input must be a string or a list of input item dicts.")

    def _validate_tools(self, tools: List[Dict[str, Any]]) -> None:
        if not isinstance(tools, list):
            raise ValueError("Tools must be a list if provided.")
        for tool in tools:
            if not isinstance(tool, dict):
                raise ValueError('Each tool must be a dict with "type" and "name".')
            if tool.get("type") != "function":
                raise ValueError('Each tool must have type "function".')
            if not isinstance(tool.get("name"), str) or not tool["name"].strip():
                raise ValueError('Each tool must have a "name" property that is a non-empty string.')

    def _validate_id(self, id: str, param_name: str) -> None:
        if not id or not isinstance(id, str) or not id.strip():
            raise ValueError(f"{param_name} must be a non-empty string.")
        if len(id) > 1024:
            raise ValueError(f"{param_name} exceeds maximum length (1024).")

    def _fetch_json(
        self,
        path: str,
        method: str = "GET",
        body: Optional[Dict[str, Any]] = None,
    ) -> Dict[str, Any]:
        url = f"{self._base_url}{path}"
        data = json.dumps(body).encode("utf-8") if body is not None else None
        req = urllib.request.Request(
            url,
            data=data,
            method=method,
            headers={"Content-Type": "application/json"},
        )
        try:
            with urllib.request.urlopen(req) as resp:
                return json.loads(resp.read().decode("utf-8"))
        except urllib.error.HTTPError as e:
            error_body = e.read().decode("utf-8", errors="replace")
            raise FoundryLocalException(
                f"Responses API error ({e.code}): {error_body}"
            ) from e
        except urllib.error.URLError as e:
            raise FoundryLocalException(
                f"Network error calling {method} {path}: {e.reason}"
            ) from e

    def _parse_sse_stream(
        self,
        path: str,
        body: Dict[str, Any],
    ) -> Generator[Dict[str, Any], None, None]:
        url = f"{self._base_url}{path}"
        data = json.dumps(body).encode("utf-8")
        req = urllib.request.Request(
            url,
            data=data,
            method="POST",
            headers={
                "Content-Type": "application/json",
                "Accept": "text/event-stream",
            },
        )
        try:
            resp = urllib.request.urlopen(req)
        except urllib.error.HTTPError as e:
            error_body = e.read().decode("utf-8", errors="replace")
            raise FoundryLocalException(
                f"Responses API error ({e.code}): {error_body}"
            ) from e
        except urllib.error.URLError as e:
            raise FoundryLocalException(
                f"Network error calling POST {path}: {e.reason}"
            ) from e

        try:
            buffer = ""
            for raw_line in resp:
                line = raw_line.decode("utf-8", errors="replace")
                buffer += line

                # Process complete SSE blocks (separated by double newlines)
                while "\n\n" in buffer:
                    block, buffer = buffer.split("\n\n", 1)
                    block = block.strip()
                    if not block:
                        continue

                    # Check for terminal signal
                    if block == "data: [DONE]":
                        return

                    # Parse data lines
                    data_lines = []
                    for sse_line in block.split("\n"):
                        if sse_line.startswith("data: "):
                            data_lines.append(sse_line[6:])
                        elif sse_line == "data:":
                            data_lines.append("")

                    if data_lines:
                        event_data = "\n".join(data_lines)
                        try:
                            yield json.loads(event_data)
                        except json.JSONDecodeError as e:
                            raise FoundryLocalException(
                                f"Failed to parse streaming event: {e}"
                            ) from e
        finally:
            resp.close()


def get_output_text(response: Dict[str, Any]) -> str:
    """Extracts the text content from an assistant message in a Response.

    Equivalent to OpenAI Python SDK's ``response.output_text``.

    Args:
        response: The Response object dict.

    Returns:
        The concatenated text from the first assistant message, or an empty string.
    """
    for item in response.get("output", []):
        if item.get("type") == "message" and item.get("role") == "assistant":
            content = item.get("content")
            if isinstance(content, str):
                return content
            if isinstance(content, list):
                return "".join(
                    p.get("text", "")
                    for p in content
                    if "text" in p
                )
    return ""
