# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Responses API client for Foundry Local's embedded web service.

Uses the native ``openai`` SDK to call the Responses API on Foundry Local's
OpenAI-compatible web service.  Create via
``FoundryLocalManager.create_responses_client()`` or
``model.create_responses_client(base_url)``.

Example::

    manager.start_web_service()
    client = manager.create_responses_client(model.id)

    # Non-streaming
    response = client.create("Hello, world!")
    print(response.output_text)

    # Streaming
    client.create_streaming("Tell me a story", lambda event: print(event))
"""

from __future__ import annotations

import logging
from typing import Any, Callable, Iterator, Optional, Union

from openai import OpenAI

logger = logging.getLogger(__name__)


class ResponsesClientSettings:
    """Default settings applied to every request made by a :class:`ResponsesClient`.

    Per-call keyword arguments passed to :meth:`ResponsesClient.create` override
    these defaults.  Attribute names match the OpenAI Responses API parameters
    (snake_case).
    """

    def __init__(
        self,
        instructions: Optional[str] = None,
        temperature: Optional[float] = None,
        top_p: Optional[float] = None,
        max_output_tokens: Optional[int] = None,
        frequency_penalty: Optional[float] = None,
        presence_penalty: Optional[float] = None,
        tool_choice: Optional[Any] = None,
        truncation: Optional[str] = None,
        parallel_tool_calls: Optional[bool] = None,
        store: Optional[bool] = None,
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
        self.seed = seed

    def _as_kwargs(self) -> dict[str, Any]:
        """Return non-None settings as keyword arguments for the openai SDK."""
        return {
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
                "seed": self.seed,
            }.items() if v is not None
        }


class ResponsesClient:
    """Client for the OpenAI Responses API served by Foundry Local.

    Backed by the native ``openai`` SDK pointed at the local web service.
    Create via :meth:`FoundryLocalManager.create_responses_client` or
    :meth:`model.create_responses_client`.

    Args:
        base_url: Base URL of the Foundry Local web service (e.g.
            ``"http://127.0.0.1:5273"``).  Do **not** include ``/v1`` — it is
            appended automatically.  Trailing slashes are stripped.
        model_id: Default model ID.  Can be overridden per-request via the
            ``model`` keyword argument to :meth:`create`.
    """

    def __init__(self, base_url: str, model_id: Optional[str] = None):
        if not base_url or not isinstance(base_url, str) or not base_url.strip():
            raise ValueError("base_url must be a non-empty string.")
        openai_base = base_url.rstrip("/") + "/v1"
        self._client = OpenAI(base_url=openai_base, api_key="notneeded")
        self._model_id = model_id
        self.settings = ResponsesClientSettings()

    # =========================================================================
    # Public API
    # =========================================================================

    def create(self, input: Union[str, list], **options: Any) -> Any:  # noqa: A002
        """Create a model response (non-streaming).

        Args:
            input: A string prompt or a list of Responses API input items.
                Each dict item must have a ``"type"`` field (e.g.
                ``{"type": "message", "role": "user", "content": [...]}``)..
            **options: Additional parameters forwarded to
                ``openai.responses.create``.  Pass ``model="..."`` to override
                the constructor default.

        Returns:
            An ``openai.types.responses.Response`` object.  Use
            ``.output_text`` for the assistant text, ``.output`` for the full
            item list, and ``.id`` for chaining with ``previous_response_id``.

        Raises:
            ValueError: If ``input`` is invalid or no model is specified.
            openai.OpenAIError: On API or network errors.
        """
        model = options.pop("model", None) or self._model_id
        self._require_model(model)
        kwargs = {**self.settings._as_kwargs(), **options}
        return self._client.responses.create(model=model, input=input, **kwargs)

    def create_streaming(
        self,
        input: Union[str, list],  # noqa: A002
        callback: Callable[[Any], None],
        **options: Any,
    ) -> None:
        """Create a model response with streaming.

        Each event object from the openai stream is delivered to *callback*.

        Args:
            input: A string prompt or a list of Responses API input items.
            callback: Called for each streaming event.  Events are typed
                ``openai`` SDK objects with a ``.type`` attribute.
            **options: Additional parameters forwarded to
                ``openai.responses.create``.

        Raises:
            ValueError: If ``input`` is invalid or *callback* is not callable.
            openai.OpenAIError: On API or network errors.
        """
        if not callable(callback):
            raise ValueError("callback must be a callable.")
        model = options.pop("model", None) or self._model_id
        self._require_model(model)
        kwargs = {**self.settings._as_kwargs(), **options}
        with self._client.responses.create(model=model, input=input, stream=True, **kwargs) as stream:
            for event in stream:
                callback(event)

    def stream(self, input: Union[str, list], **options: Any) -> Iterator[Any]:  # noqa: A002
        """Create a model response and return an iterator of streaming events.

        This is a generator-style alternative to :meth:`create_streaming` that
        yields each event instead of using a callback.

        Args:
            input: A string prompt or a list of Responses API input items.
            **options: Additional parameters forwarded to
                ``openai.responses.create``.

        Yields:
            Streaming event objects from the openai SDK.

        Raises:
            ValueError: If no model is specified.
            openai.OpenAIError: On API or network errors.
        """
        model = options.pop("model", None) or self._model_id
        self._require_model(model)
        kwargs = {**self.settings._as_kwargs(), **options}
        with self._client.responses.create(model=model, input=input, stream=True, **kwargs) as stream:
            yield from stream

    def get(self, response_id: str) -> Any:
        """Retrieve a stored response by ID.

        Args:
            response_id: The ID of the response to retrieve.

        Returns:
            An ``openai.types.responses.Response`` object.
        """
        self._validate_id(response_id, "response_id")
        return self._client.responses.retrieve(response_id)

    def delete(self, response_id: str) -> Any:
        """Delete a stored response by ID.

        Args:
            response_id: The ID of the response to delete.

        Returns:
            The deletion result object.
        """
        self._validate_id(response_id, "response_id")
        return self._client.responses.delete(response_id)

    def cancel(self, response_id: str) -> Any:
        """Cancel an in-progress response.

        Args:
            response_id: The ID of the response to cancel.

        Returns:
            The cancelled ``openai.types.responses.Response`` object.
        """
        self._validate_id(response_id, "response_id")
        return self._client.responses.cancel(response_id)

    def get_input_items(self, response_id: str) -> Any:
        """Retrieve the input items for a stored response.

        Args:
            response_id: The ID of the response.

        Returns:
            A paginated list of input items.
        """
        self._validate_id(response_id, "response_id")
        return self._client.responses.input_items.list(response_id)

    def close(self) -> None:
        """Close the underlying OpenAI HTTP client and release resources."""
        self._client.close()

    def __enter__(self) -> "ResponsesClient":
        return self

    def __exit__(self, *args: Any) -> None:
        self.close()

    # =========================================================================
    # Internal helpers
    # =========================================================================

    def _require_model(self, model: Optional[str]) -> None:
        if not model or not isinstance(model, str) or not model.strip():
            raise ValueError(
                "model must be specified either in the constructor via "
                "create_responses_client(model_id) or as an options keyword argument."
            )

    def _validate_id(self, value: Any, param: str) -> None:
        if not isinstance(value, str) or not value.strip():
            raise ValueError(f"{param} must be a non-empty string.")
        if len(value) > 1024:
            raise ValueError(f"{param} exceeds the maximum length of 1024 characters.")
