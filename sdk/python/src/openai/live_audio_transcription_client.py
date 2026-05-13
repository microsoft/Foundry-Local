# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Live audio transcription streaming session.

Provides :class:`LiveAudioTranscriptionSession` — a push-based streaming
session for real-time audio-to-text transcription via ONNX Runtime GenAI.

Error handling
--------------
All session operations raise :class:`FoundryLocalException` on failure.
Common failure modes:

- **Session lifecycle errors** — raised by :meth:`start` / :meth:`stop` /
  :meth:`append` / :meth:`get_transcription_stream` when called in an
  invalid state (e.g. calling ``start()`` twice, or ``append()`` before
  ``start()``).  Message contains ``"already started"`` /
  ``"No active streaming session"``.
- **Native core errors** — raised when the native Core returns an error
  response (e.g. ``audio_stream_start`` fails).  Message has the form
  ``"Error starting/stopping audio stream session: <native error>"``.
- **Push loop fatal errors** — raised from inside
  :meth:`get_transcription_stream` when a chunk push fails.  Message has
  the form ``"Push failed (code=<code>): <native error>"`` where
  ``<code>`` is parsed from :class:`CoreErrorResponse` (e.g.
  ``ASR_SESSION_NOT_FOUND``, ``BUSY``, or ``UNKNOWN`` if the error is
  unstructured).  Once a push loop fatal error occurs, the session is
  terminated and must be re-created.
"""

from __future__ import annotations

import collections
import logging
import threading
from typing import Generator, Optional

from ..detail.core_interop import CoreInterop, InteropRequest
from ..exception import FoundryLocalException
from .live_audio_transcription_types import (
    CoreErrorResponse,
    LiveAudioTranscriptionOptions,
    LiveAudioTranscriptionResponse,
)

logger = logging.getLogger(__name__)

_SENTINEL = object()


class _CancelledError(Exception):
    """Raised by :class:`_CancellableQueue` when the queue is cancelled."""


class _CancellableQueue:
    """Bounded queue with instant cancellation via :class:`threading.Condition`.

    Unlike :class:`queue.Queue`, blocking :meth:`get` / :meth:`put` calls are
    interrupted **immediately** when :meth:`cancel` is called — the underlying
    ``Condition.notify_all()`` wakes every waiting thread with zero polling
    overhead.
    """

    def __init__(self, maxsize: int = 0):
        self._deque: collections.deque = collections.deque()
        self._maxsize = maxsize  # 0 = unbounded
        self._cond = threading.Condition(threading.Lock())
        self._cancelled = False

    def put(self, item: object) -> None:
        """Enqueue *item*, blocking if at capacity.

        Raises :class:`_CancelledError` immediately if the queue has been
        cancelled (even when space is available).
        """
        with self._cond:
            if self._cancelled:
                raise _CancelledError()
            while 0 < self._maxsize <= len(self._deque):
                self._cond.wait()
                if self._cancelled:
                    raise _CancelledError()
            self._deque.append(item)
            self._cond.notify()  # wake one blocked get()

    def get(self) -> object:
        """Dequeue one item, blocking if empty.

        Raises :class:`_CancelledError` immediately if the queue has been
        cancelled (even when items are available).
        """
        with self._cond:
            if self._cancelled:
                raise _CancelledError()
            while len(self._deque) == 0:
                self._cond.wait()
                if self._cancelled:
                    raise _CancelledError()
            item = self._deque.popleft()
            if self._maxsize > 0:
                self._cond.notify()  # wake one blocked put()
            return item

    def cancel(self) -> None:
        """Cancel all current and future blocked :meth:`get` / :meth:`put` calls."""
        with self._cond:
            self._cancelled = True
            self._cond.notify_all()


class LiveAudioTranscriptionSession:
    """Session for real-time audio streaming ASR (Automatic Speech Recognition).

    Audio data from a microphone (or other source) is pushed in as PCM chunks,
    and transcription results are returned as a synchronous generator.

    Created via :meth:`AudioClient.create_live_transcription_session`.

    Thread safety
    -------------
    :meth:`append` can be called from any thread (including high-frequency
    audio callbacks).  Pushes are internally serialized via a bounded queue
    to prevent unbounded memory growth and ensure ordering.

    Example::

        session = audio_client.create_live_transcription_session()
        session.settings.sample_rate = 16000
        session.settings.channels = 1
        session.settings.language = "en"

        session.start()

        # Push audio from a microphone callback (thread-safe)
        session.append(pcm_bytes)

        # Read results as they arrive
        for result in session.get_transcription_stream():
            print(result.content[0].text, end="", flush=True)

        session.stop()
    """

    def __init__(
        self,
        model_id: str,
        core_interop: CoreInterop,
        cancel_event: Optional[threading.Event] = None,
    ):
        self._model_id = model_id
        self._core_interop = core_interop

        # Session-level cancellation event. Set from any thread (e.g., a SIGINT
        # handler) to cancel in-flight start/append/stop and unblock the
        # transcription generator. Methods also accept an optional per-call
        # cancel_event; setting EITHER will cancel.
        self._cancel_event = cancel_event

        # Public settings — mutable until start()
        self.settings = LiveAudioTranscriptionOptions()

        # Session state — protected by _lock
        self._lock = threading.Lock()
        self._session_handle: Optional[str] = None
        self._started = False
        self._stopped = False

        # Frozen settings snapshot
        self._active_settings: Optional[LiveAudioTranscriptionOptions] = None

        # Output queue: push loop writes, user reads via get_transcription_stream
        self._output_queue: Optional[_CancellableQueue] = None

        # Internal push queue: user writes audio chunks, background loop drains to native core
        self._push_queue: Optional[_CancellableQueue] = None
        self._push_thread: Optional[threading.Thread] = None
        self._cancel_watcher_thread: Optional[threading.Thread] = None

    def _is_cancelled(self) -> bool:
        """True if the session-level cancel_event is set."""
        return self._cancel_event is not None and self._cancel_event.is_set()

    def start(self) -> None:
        """Start a real-time audio streaming session.

        Must be called before :meth:`append` or :meth:`get_transcription_stream`.
        Settings are frozen after this call.

        Cancellation is configured once via the ``cancel_event`` passed to
        :meth:`AudioClient.create_live_transcription_session`. If that event
        is already set, this raises :class:`FoundryLocalException` before
        the native session is created.

        Raises:
            FoundryLocalException: If the session is already started
                (message contains ``"already started"``), if the native
                core fails to start the stream (message has form
                ``"Error starting audio stream session: <native error>"``),
                or if cancellation was requested before the call.
        """
        if self._is_cancelled():
            raise FoundryLocalException("start() cancelled before the session was created.")
        with self._lock:
            if self._started:
                raise FoundryLocalException(
                    "Streaming session already started. Call stop() first."
                )

            # Freeze settings
            self._active_settings = self.settings.snapshot()

            self._output_queue = _CancellableQueue()
            self._push_queue = _CancellableQueue(
                maxsize=self._active_settings.push_queue_capacity
            )

            request = InteropRequest(
                params={
                    "Model": self._model_id,
                    "SampleRate": str(self._active_settings.sample_rate),
                    "Channels": str(self._active_settings.channels),
                    "BitsPerSample": str(self._active_settings.bits_per_sample),
                }
            )

            if self._active_settings.language is not None:
                request.params["Language"] = self._active_settings.language

            response = self._core_interop.start_audio_stream(request)

            if response.error is not None:
                raise FoundryLocalException(
                    f"Error starting audio stream session: {response.error}"
                )

            self._session_handle = response.data
            if self._session_handle is None:
                raise FoundryLocalException(
                    "Native core did not return a session handle."
                )

            self._started = True
            self._stopped = False

            # Start the push loop thread (non-daemon so it blocks process
            # exit until stop() is called — aligns with FL Core's no-daemon design)
            self._push_thread = threading.Thread(target=self._push_loop, daemon=False)
            self._push_thread.start()

            # Start watcher thread: waits on cancel_event, then calls cancel()
            # on both queues so notify_all() instantly unblocks any waiting
            # get() / put().
            if self._cancel_event is not None:
                self._cancel_watcher_thread = threading.Thread(
                    target=self._cancel_watcher, daemon=True
                )
                self._cancel_watcher_thread.start()

    def append(self, pcm_data: bytes) -> None:
        """Push a chunk of raw PCM audio data to the streaming session.

        Can be called from any thread (including audio device callbacks).
        Chunks are internally queued and serialized to the native core.

        The data is copied to avoid issues if the caller reuses the buffer.

        If the internal push queue is full (capacity controlled by
        :attr:`LiveAudioTranscriptionOptions.push_queue_capacity`, default
        100), this method **blocks** until space is available
        (backpressure).  This prevents unbounded memory growth when the
        native core falls behind real-time.

        Cancellation is configured once via the ``cancel_event`` passed to
        :meth:`AudioClient.create_live_transcription_session`. If that event
        fires while ``append`` is blocked on backpressure, the internal
        watcher thread instantly unblocks the call via
        ``Condition.notify_all()`` and :class:`FoundryLocalException` is
        raised **without enqueueing the chunk** (no risk of late delivery
        to native core).

        Args:
            pcm_data: Raw PCM audio bytes matching the configured format.

        Raises:
            FoundryLocalException: If the session is not active (not
                started, or already stopped — message contains
                ``"No active streaming session"``), or if the call was
                cancelled before the chunk was enqueued.
        """
        # Copy the data to avoid issues if the caller reuses the buffer
        data_copy = bytes(pcm_data)

        with self._lock:
            if not self._started or self._stopped:
                raise FoundryLocalException(
                    "No active streaming session. Call start() first."
                )

            push_queue = self._push_queue
            if push_queue is None:
                raise FoundryLocalException(
                    "No active streaming session. Call start() first."
                )

        # Enqueue the chunk. _CancellableQueue.put() blocks if at capacity
        # and raises _CancelledError instantly if the watcher thread fires
        # cancel() — no polling needed.
        try:
            push_queue.put(data_copy)
        except _CancelledError:
            raise FoundryLocalException("append() cancelled before the chunk was enqueued.")

    def get_transcription_stream(
        self,
    ) -> Generator[LiveAudioTranscriptionResponse, None, None]:
        """Get the stream of transcription results.

        Results arrive as the native ASR engine processes audio data.
        The generator completes when :meth:`stop` is called and all
        remaining audio has been processed.

        Cancellation is configured once via the ``cancel_event`` passed to
        :meth:`AudioClient.create_live_transcription_session`. If that event
        fires, iteration ends cleanly (the generator returns instead of
        raising).

        After :meth:`stop` completes, calling this method again returns
        an empty generator (the sentinel is still on the queue) — matching
        the C# / JS SDK behavior.

        Yields:
            Transcription results as ``LiveAudioTranscriptionResponse`` objects.

        Raises:
            FoundryLocalException: If no active streaming session exists
                (``start()`` was never called).  Also raised from inside
                the iterator if a push fails — message has form
                ``"Push failed (code=<code>): <native error>"`` where
                ``<code>`` is parsed via
                :meth:`CoreErrorResponse.try_parse` (e.g.
                ``ASR_SESSION_NOT_FOUND``, ``BUSY``).  Once raised, the
                session is terminated.
        """
        q = self._output_queue
        if q is None:
            raise FoundryLocalException(
                "No active streaming session. Call start() first."
            )

        # _CancellableQueue.get() blocks until an item arrives and raises
        # _CancelledError instantly when the watcher thread fires cancel()
        # — no polling, no separate fast-path needed.
        while True:
            try:
                item = q.get()
            except _CancelledError:
                return  # clean exit on cancellation
            if item is _SENTINEL:
                break
            if isinstance(item, Exception):
                raise item
            yield item

    def stop(self) -> None:
        """Signal end-of-audio and stop the streaming session.

        Any remaining buffered audio in the push queue will be drained to
        native core first.  Final results are delivered through
        :meth:`get_transcription_stream` before it completes.

        Cancellation is configured once via the ``cancel_event`` passed to
        :meth:`AudioClient.create_live_transcription_session`. If that event
        fires while ``stop`` is waiting for the drain to finish, the
        watcher thread instantly unblocks the push loop so ``stop``
        returns promptly. The native session is still finalized so
        resources are released.

        Idempotent: calling ``stop()`` on a session that was never started
        or has already been stopped is a no-op.

        Raises:
            FoundryLocalException: If the native core fails to stop the
                stream cleanly.  Message has form
                ``"Error stopping audio stream session: <native error>"``.
                Note: the SDK still completes its local cleanup before
                raising, so the session is left in a fully-stopped state.
        """
        with self._lock:
            if not self._started or self._stopped:
                return  # already stopped or never started

            self._stopped = True

        # 1. Signal push loop to finish (put sentinel)
        try:
            self._push_queue.put(_SENTINEL)
        except _CancelledError:
            pass  # push loop exits on cancel; sentinel not needed

        # 2. Wait for push loop to finish draining. On cancel the watcher
        #    thread already cancelled the push queue, so the loop exits
        #    promptly and join() returns quickly — no polling needed.
        if self._push_thread is not None:
            self._push_thread.join()

        # 3. Tell native core to flush and finalize
        request = InteropRequest(params={"SessionHandle": self._session_handle})
        response = self._core_interop.stop_audio_stream(request)

        # Parse final transcription from stop response
        if response.data:
            try:
                final_result = LiveAudioTranscriptionResponse.from_json(response.data)
                text = final_result.content[0].text if final_result.content else ""
                if text:
                    try:
                        self._output_queue.put(final_result)
                    except _CancelledError:
                        pass  # generator already returned on cancel
            except Exception as parse_ex:
                logger.debug(
                    "Could not parse stop response as transcription result: %s",
                    parse_ex,
                )

        # 4. Complete the output queue
        try:
            self._output_queue.put(_SENTINEL)
        except _CancelledError:
            pass  # generator already returned on cancel

        # 5. Clean up — keep _output_queue intact so that
        # get_transcription_stream() returns an empty stream (matching C#/JS
        # behavior where the completed stream remains readable).
        self._session_handle = None
        self._started = False

        if response.error is not None:
            raise FoundryLocalException(
                f"Error stopping audio stream session: {response.error}"
            )

    def _push_loop(self) -> None:
        """Internal loop that drains the push queue and sends chunks to native core.

        Terminates the session on any native error.
        """
        try:
            while True:
                audio_data = self._push_queue.get()
                if audio_data is _SENTINEL:
                    break

                request = InteropRequest(params={"SessionHandle": self._session_handle})
                response = self._core_interop.push_audio_data(request, audio_data)

                if response.error is not None:
                    error_info = CoreErrorResponse.try_parse(response.error)
                    code = error_info.code if error_info else "UNKNOWN"
                    fatal_ex = FoundryLocalException(
                        f"Push failed (code={code}): {response.error}"
                    )
                    logger.error(
                        "Terminating push loop due to push failure: %s", response.error
                    )
                    try:
                        self._output_queue.put(fatal_ex)
                        self._output_queue.put(_SENTINEL)
                    except _CancelledError:
                        pass
                    return

                # Parse transcription result from push response and surface it
                if response.data:
                    try:
                        transcription = LiveAudioTranscriptionResponse.from_json(
                            response.data
                        )
                        text = (
                            transcription.content[0].text
                            if transcription.content
                            else ""
                        )
                        if text:
                            self._output_queue.put(transcription)
                    except _CancelledError:
                        raise  # let outer handler deal with cancellation
                    except Exception as parse_ex:
                        # Non-fatal: log and continue
                        logger.debug(
                            "Could not parse push response as transcription: %s",
                            parse_ex,
                        )
        except _CancelledError:
            # Session cancelled — exit silently. The watcher thread already
            # cancelled the output queue, so the generator returns cleanly.
            return
        except Exception as ex:
            logger.error("Push loop terminated with unexpected error: %s", ex)
            fatal_ex = FoundryLocalException("Push loop terminated unexpectedly.")
            fatal_ex.__cause__ = ex
            try:
                self._output_queue.put(fatal_ex)
                self._output_queue.put(_SENTINEL)
            except _CancelledError:
                pass

    def _cancel_watcher(self) -> None:
        """Daemon thread: waits for cancel_event, then cancels both queues.

        A single ``notify_all()`` per queue instantly unblocks every thread
        waiting in ``get()`` or ``put()`` — no polling interval required.

        Lifecycle note: if ``cancel_event`` is never set (i.e., the session
        completes via a normal ``stop()``), this thread remains blocked
        on ``cancel_event.wait()`` for the lifetime of the user's event
        object. It is a daemon, so it does NOT block process exit; but it
        does retain a reference to ``self``, which is acceptable because
        sessions are normally long-lived (one per process). Python's stdlib
        has no clean primitive for "wait on either of two events" without
        either polling or spawning extra helpers, so we accept this minor
        leak in exchange for zero polling overhead during normal operation.
        """
        self._cancel_event.wait()
        if self._push_queue is not None:
            self._push_queue.cancel()
        if self._output_queue is not None:
            self._output_queue.cancel()

    # --- Context manager support ---

    def __enter__(self) -> LiveAudioTranscriptionSession:
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        try:
            if self._started and not self._stopped:
                self.stop()
        except Exception as ex:
            logger.warning("Error during context manager cleanup: %s", ex)
