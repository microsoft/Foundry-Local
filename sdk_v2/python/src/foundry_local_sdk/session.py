# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
from __future__ import annotations

import abc
import queue
import threading
from typing import TYPE_CHECKING, Iterator

if TYPE_CHECKING:
    from foundry_local_sdk.imodel import IModel
    from foundry_local_sdk.items import Item
    from foundry_local_sdk.request import Request
    from foundry_local_sdk.response import Response

_API_VERSION = 1  # FOUNDRY_LOCAL_API_VERSION

# Sentinel placed on the stream queue by the background thread when inference finishes.
_DONE = object()


class _StreamError:
    """Wraps an exception propagated from the background inference thread."""

    def __init__(self, exc: BaseException) -> None:
        self.exc = exc


class Session(abc.ABC):
    """Base inference session wrapping a native flSession*.

    Provides synchronous request processing, session-level options, and
    synchronous streaming via ``set_streaming`` + ``process_streaming_request``.

    This class is abstract: instantiate a modality-specific subclass
    (``ChatSession``, ``AudioSession``, ``EmbeddingsSession``) rather than
    ``Session`` directly.
    """

    def __new__(cls, *args, **kwargs):  # noqa: ARG004 — args forwarded to __init__
        if cls is Session:
            raise TypeError(
                "Session is an abstract base class; instantiate a modality-specific "
                "subclass such as ChatSession, AudioSession, or EmbeddingsSession."
            )
        return super().__new__(cls)

    def __init__(self, model: "IModel") -> None:
        # Initialise lifecycle flags FIRST so that if anything below raises,
        # __del__ sees a fully-constructed (but already-closed) object and
        # cleanly no-ops instead of AttributeError'ing inside the GC.
        self._closed = True
        self._ptr = None
        self._stream_thread = None
        self._stream_request = None

        from foundry_local_sdk._native import ffi
        from foundry_local_sdk._native.api import api
        from foundry_local_sdk.imodel import _ModelImpl

        if not isinstance(model, _ModelImpl):
            raise TypeError("model must be a native IModel instance")

        out = ffi.new("flSession**")
        api.check_status(api.inference.Session_Create(model._ptr, out))
        self._ptr = out[0]
        self._closed = False

        # Streaming state — populated by set_streaming(True).
        self._streaming_enabled = False
        self._streaming_callback = None  # cffi callback object; held to prevent GC
        self._stream_queue: queue.Queue | None = None

        # Non-blocking gate used to detect (not serialize) concurrent streaming requests on the same session.
        # The native session has a single _stream_queue / callback path that cannot multiplex two in-flight streams;
        # the second caller must fail fast rather than silently cross-pollinate items into the first caller's iterator.
        self._streaming_in_flight = threading.Lock()

    def _check_open(self) -> None:
        from foundry_local_sdk.exception import FoundryLocalException

        if self._closed:
            raise FoundryLocalException(
                f"{type(self).__name__} has been closed and can no longer be used."
            )

    def set_options(self, options: dict[str, str]) -> "Session":
        """Set session-level inference options. Applies to all subsequent process_request calls."""
        self._check_open()
        from foundry_local_sdk._native import ffi
        from foundry_local_sdk._native.api import api

        kvp_out = ffi.new("flKeyValuePairs**")
        api.root.CreateKeyValuePairs(kvp_out)
        kvp = kvp_out[0]
        try:
            for key, value in options.items():
                api.root.AddKeyValuePair(kvp, key.encode("utf-8"), str(value).encode("utf-8"))
            api.check_status(api.inference.Session_SetOptions(self._ptr, kvp))
        finally:
            api.root.KeyValuePairs_Release(kvp)
        return self

    def set_streaming(self, enabled: bool) -> "Session":
        """Install or remove the native streaming callback on this session.

        Must be called with ``True`` before ``process_streaming_request``.
        The callback remains installed across requests until disabled or the
        session is closed.

        Returns:
            self (fluent).
        """
        self._check_open()
        from foundry_local_sdk._native import ffi
        from foundry_local_sdk._native.api import api
        from foundry_local_sdk.items import Item

        if enabled and not self._streaming_enabled:
            # Build the cffi callback as a closure over self so it can reach
            # _stream_queue without going through user_data.
            # The object is stored on self to prevent the GC from collecting it
            # while the native session still holds the C function pointer.
            def _cb(data, user_data):
                q = self._stream_queue
                if q is None:
                    return 0

                try:
                    if data.item_queue != ffi.NULL:
                        item_out = ffi.new("flItem**")
                        while api.item.ItemQueue_TryPop(data.item_queue, item_out):
                            # Ownership transferred to the Python Item wrapper.
                            q.put(Item.from_native(item_out[0], owns=True))
                except Exception as exc:
                    q.put(_StreamError(exc))
                    return 1

                return 0

            self._streaming_callback = ffi.callback("int(flStreamingCallbackData, void *)", _cb)
            self._streaming_enabled = True
            api.check_status(
                api.inference.Session_SetStreamingCallback(
                    self._ptr, self._streaming_callback, ffi.NULL
                )
            )

        elif not enabled and self._streaming_enabled:
            # Passing a NULL function pointer uninstalls the callback.
            api.check_status(
                api.inference.Session_SetStreamingCallback(
                    self._ptr, ffi.cast("flStreamingCallback", 0), ffi.NULL
                )
            )
            self._streaming_callback = None
            self._streaming_enabled = False

        return self

    def process_streaming_request(self, request: "Request") -> "Iterator[Item]":
        """Run a request and yield items as they are produced by the model.

        Runs ``Session_ProcessRequest`` in a background thread.  The native
        streaming callback populates an unbounded ``queue.Queue``; this
        generator drains it synchronously.

        Abandoning the generator (``break`` / exception) automatically calls
        ``request.cancel()`` and joins the background thread so the session
        is ready for the next request.

        Requires ``set_streaming(True)`` to have been called first.

        Args:
            request: The inference request. Do not reuse while streaming.

        Yields:
            ``Item`` instances in production order.

        Raises:
            FoundryLocalException: If streaming was not enabled, or if the
                background thread encounters a native error.
        """
        self._check_open()
        from foundry_local_sdk._native import ffi
        from foundry_local_sdk._native.api import api
        from foundry_local_sdk.exception import FoundryLocalException

        if not self._streaming_enabled:
            raise FoundryLocalException(
                "Streaming not enabled. Call set_streaming(True) before process_streaming_request."
            )

        # Detect concurrent streaming on the same session — there is exactly one native callback / _stream_queue slot,
        # so a second caller would have its items interleaved into the first caller's iterator.
        # The lock is released by the generator's finally on normal completion, break, exception, GC, or .close().
        if not self._streaming_in_flight.acquire(blocking=False):
            raise FoundryLocalException(
                "Concurrent streaming requests on the same session are not supported. "
                "Drain or cancel the in-flight stream before starting another."
            )

        try:
            q: queue.Queue = queue.Queue()
            self._stream_queue = q

            def _run() -> None:
                try:
                    out = ffi.new("flResponse**")
                    api.check_status(api.inference.Session_ProcessRequest(self._ptr, request._ptr, out))
                    # The streaming items are already in the queue via the callback;
                    # release the shell Response object immediately.
                    api.inference.Response_Release(out[0])
                except Exception as exc:
                    q.put(_StreamError(exc))
                finally:
                    self._stream_queue = None
                    q.put(_DONE)

            t = threading.Thread(target=_run, daemon=True)
            self._stream_thread = t
            self._stream_request = request
            t.start()

            completed = False
            try:
                while True:
                    msg = q.get()
                    if msg is _DONE:
                        completed = True
                        break
                    if isinstance(msg, _StreamError):
                        # The worker thread has already exited and posted _DONE (or is about to).
                        # Drain it and mark completed so the finally block does not call
                        # request.cancel() on an already-finished request.
                        while q.get() is not _DONE:
                            pass
                        completed = True
                        raise msg.exc
                    yield msg
            finally:
                try:
                    # Only cancel if the generator was abandoned before inference finished
                    # (break, exception, or GC). Cancelling after a clean _DONE would hit
                    # the native layer with a spurious cancel and produce misleading log output.
                    if not completed:
                        request.cancel()
                    t.join()
                finally:
                    self._stream_thread = None
                    self._stream_request = None
        finally:
            self._streaming_in_flight.release()

    def process_request(self, request: "Request") -> "Response":
        """Run the request synchronously and return the complete response."""
        self._check_open()
        from foundry_local_sdk._native import ffi
        from foundry_local_sdk._native.api import api
        from foundry_local_sdk.response import Response

        out = ffi.new("flResponse**")
        api.check_status(api.inference.Session_ProcessRequest(self._ptr, request._ptr, out))
        return Response(out[0])

    def _close(self) -> None:
        # Defensive: subclasses (ChatSession, AudioSession, EmbeddingsSession) validate
        # the model task BEFORE calling super().__init__(), so a validation failure leaves
        # a partially-constructed object that the GC will still try to finalise. Use
        # getattr so __del__ -> _close() no-ops cleanly instead of AttributeError'ing.
        if getattr(self, "_closed", True) or getattr(self, "_ptr", None) is None:
            return

        # If a streaming request is in flight, wind it down before Session_Release —
        # releasing while the worker is inside Session_ProcessRequest is a native
        # use-after-free.
        t = getattr(self, "_stream_thread", None)
        if t is not None and t.is_alive():
            req = getattr(self, "_stream_request", None)
            if req is not None:
                try:
                    req.cancel()
                except Exception:
                    pass
            # Bounded wait — if the worker is wedged past this, releasing is still
            # safer than blocking the caller indefinitely on what may be a runaway thread.
            t.join(timeout=5.0)

        try:
            from foundry_local_sdk._native.api import api

            api.inference.Session_Release(self._ptr)
        except Exception:
            pass
        self._ptr = None
        self._closed = True

    def __enter__(self) -> "Session":
        return self

    def __exit__(self, *_) -> None:
        self._close()

    def __del__(self) -> None:
        self._close()


class ChatSession(Session):
    """Inference session for chat-completion and vision-language-chat models.

    Validates the model task at construction time. Supports tool definitions,
    turn count inspection, and turn undo.
    """

    _SUPPORTED_TASKS = frozenset({"chat-completion", "vision-language-chat"})

    def __init__(self, model: "IModel") -> None:
        task = model.info.task
        if task not in self._SUPPORTED_TASKS:
            raise ValueError(
                f"ChatSession requires a model with task 'chat-completion' or "
                f"'vision-language-chat', but got {task!r}."
            )
        super().__init__(model)

    def add_tool_definition(self, name: str, description: str, json_schema: str) -> "ChatSession":
        """Register a tool so the model can request tool calls. Returns self (fluent)."""
        from foundry_local_sdk._native import ffi
        from foundry_local_sdk._native.api import api

        # Keep cffi temporaries as named locals so they outlive the native call.
        c_name = ffi.new("char[]", name.encode("utf-8") + b"\x00")
        c_desc = ffi.new("char[]", description.encode("utf-8") + b"\x00")
        c_schema = ffi.new("char[]", json_schema.encode("utf-8") + b"\x00")

        tool_def = ffi.new("flToolDefinition*")
        tool_def.version = _API_VERSION
        tool_def.name = c_name
        tool_def.description = c_desc
        tool_def.json_schema = c_schema

        api.check_status(api.inference.Session_AddToolDefinition(self._ptr, tool_def))
        return self

    @property
    def turn_count(self) -> int:
        """Number of completed turns accumulated in this session."""
        from foundry_local_sdk._native.api import api

        return int(api.inference.Session_GetTurnCount(self._ptr))

    def undo_turns(self, count: int) -> None:
        """Remove the last `count` turns from session history."""
        from foundry_local_sdk._native.api import api

        api.check_status(api.inference.Session_UndoTurns(self._ptr, count))


class AudioSession(Session):
    """Inference session for automatic-speech-recognition models.

    Accepts ``AudioItem`` input and produces ``TextItem`` output.
    Validates the model task at construction time.
    """

    def __init__(self, model: "IModel") -> None:
        task = model.info.task
        if task != "automatic-speech-recognition":
            raise ValueError(
                f"AudioSession requires a model with task 'automatic-speech-recognition', "
                f"but got {task!r}."
            )
        super().__init__(model)


class EmbeddingsSession(Session):
    """Inference session for text-embedding models.

    Accepts ``TextItem`` inputs and produces one ``TensorItem`` per input
    containing the embedding vector. Stateless — multiple requests can be
    processed concurrently against the same loaded model.
    Validates the model task at construction time.
    """

    def __init__(self, model: "IModel") -> None:
        task = model.info.task
        if task != "embeddings":
            raise ValueError(
                f"EmbeddingsSession requires a model with task 'embeddings', "
                f"but got {task!r}."
            )
        super().__init__(model)
