# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Live audio transcription streaming session.

Provides :class:`LiveAudioTranscriptionSession` — a push-based streaming
session for real-time audio-to-text transcription via ONNX Runtime GenAI.
"""

from __future__ import annotations

import logging
import queue
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
        self._output_queue: Optional[queue.Queue] = None

        # Internal push queue: user writes audio chunks, background loop drains to native core
        self._push_queue: Optional[queue.Queue] = None
        self._push_thread: Optional[threading.Thread] = None

    def _is_cancelled(self, call_event: Optional[threading.Event]) -> bool:
        """True if EITHER the per-call event or the session-level event is set."""
        if call_event is not None and call_event.is_set():
            return True
        if self._cancel_event is not None and self._cancel_event.is_set():
            return True
        return False

    def start(self, cancel_event: Optional[threading.Event] = None) -> None:
        """Start a real-time audio streaming session.

        Must be called before :meth:`append` or :meth:`get_transcription_stream`.
        Settings are frozen after this call.

        Args:
            cancel_event: Optional per-call cancellation event. Composed with
                the session-level event passed to the constructor — EITHER
                being set raises :class:`FoundryLocalException` (CancelledError
                semantics) before the native session is created.

        Raises:
            FoundryLocalException: If the session is already started, the
                native core returns an error, or cancellation was requested.
        """
        if self._is_cancelled(cancel_event):
            raise FoundryLocalException("start() cancelled before the session was created.")
        with self._lock:
            if self._started:
                raise FoundryLocalException(
                    "Streaming session already started. Call stop() first."
                )

            # Freeze settings
            self._active_settings = self.settings.snapshot()

            self._output_queue = queue.Queue()
            self._push_queue = queue.Queue(
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

    def append(
        self,
        pcm_data: bytes,
        cancel_event: Optional[threading.Event] = None,
    ) -> None:
        """Push a chunk of raw PCM audio data to the streaming session.

        Can be called from any thread (including audio device callbacks).
        Chunks are internally queued and serialized to the native core.

        The data is copied to avoid issues if the caller reuses the buffer.

        Args:
            pcm_data: Raw PCM audio bytes matching the configured format.
            cancel_event: Optional per-call cancellation event. Composed with
                the session-level event — EITHER being set unblocks a
                backpressured ``append`` and raises :class:`FoundryLocalException`
                **without enqueueing the chunk** (no risk of late delivery to
                native core).

        Raises:
            FoundryLocalException: If no active streaming session exists or
                the call was cancelled.
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

        # Fast-path: no cancellation event configured anywhere -> use the
        # original blocking put() so we don't add per-call polling overhead.
        if cancel_event is None and self._cancel_event is None:
            push_queue.put(data_copy)
            return

        # Cancellation-aware path: poll with a small timeout so we can
        # surface a cancel set from another thread without enqueuing the chunk.
        # Performed outside the lock to avoid blocking stop() and other
        # state transitions while waiting for queue space.
        while True:
            if self._is_cancelled(cancel_event):
                raise FoundryLocalException("append() cancelled before the chunk was enqueued.")
            try:
                push_queue.put(data_copy, timeout=0.1)
                return
            except queue.Full:
                continue

    def get_transcription_stream(
        self,
        cancel_event: Optional[threading.Event] = None,
    ) -> Generator[LiveAudioTranscriptionResponse, None, None]:
        """Get the stream of transcription results.

        Results arrive as the native ASR engine processes audio data.
        The generator completes when :meth:`stop` is called and all
        remaining audio has been processed.

        Args:
            cancel_event: Optional per-call cancellation event. Composed with
                the session-level event — EITHER being set ends iteration
                cleanly (the generator returns instead of raising).

        Yields:
            Transcription results as ``LiveAudioTranscriptionResponse`` objects.

        Raises:
            FoundryLocalException: If no active streaming session exists,
                or if the push loop encountered a fatal error.
        """
        q = self._output_queue
        if q is None:
            raise FoundryLocalException(
                "No active streaming session. Call start() first."
            )

        # Fast-path with no cancel sources — use blocking get() unchanged.
        if cancel_event is None and self._cancel_event is None:
            while True:
                item = q.get()
                if item is _SENTINEL:
                    break
                if isinstance(item, Exception):
                    raise item
                yield item
            return

        # Cancellation-aware path: poll periodically so we can return cleanly
        # when either cancel source fires.
        while True:
            if self._is_cancelled(cancel_event):
                return
            try:
                item = q.get(timeout=0.1)
            except queue.Empty:
                continue
            if item is _SENTINEL:
                break
            if isinstance(item, Exception):
                raise item
            yield item

    def stop(self, cancel_event: Optional[threading.Event] = None) -> None:
        """Signal end-of-audio and stop the streaming session.

        Any remaining buffered audio in the push queue will be drained to
        native core first.  Final results are delivered through
        :meth:`get_transcription_stream` before it completes.

        Args:
            cancel_event: Optional per-call cancellation event. Composed with
                the session-level event — EITHER being set short-circuits the
                drain wait so ``stop`` returns promptly without waiting for
                the push thread to finish naturally. The native session is
                still finalized so resources are released.
        """
        with self._lock:
            if not self._started or self._stopped:
                return  # already stopped or never started

            self._stopped = True

        # 1. Signal push loop to finish (put sentinel)
        self._push_queue.put(_SENTINEL)

        # 2. Wait for push loop to finish draining. If a cancel is requested,
        #    poll with a short timeout so stop() can return promptly.
        if self._push_thread is not None:
            if cancel_event is None and self._cancel_event is None:
                self._push_thread.join()
            else:
                while self._push_thread.is_alive():
                    if self._is_cancelled(cancel_event):
                        break  # short-circuit drain — proceed to native stop
                    self._push_thread.join(timeout=0.1)

        # 3. Tell native core to flush and finalize
        request = InteropRequest(params={"SessionHandle": self._session_handle})
        response = self._core_interop.stop_audio_stream(request)

        # Parse final transcription from stop response
        if response.data:
            try:
                final_result = LiveAudioTranscriptionResponse.from_json(response.data)
                text = final_result.content[0].text if final_result.content else ""
                if text:
                    self._output_queue.put(final_result)
            except Exception as parse_ex:
                logger.debug(
                    "Could not parse stop response as transcription result: %s",
                    parse_ex,
                )

        # 4. Complete the output queue
        self._output_queue.put(_SENTINEL)

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
                    self._output_queue.put(fatal_ex)
                    self._output_queue.put(_SENTINEL)
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
                    except Exception as parse_ex:
                        # Non-fatal: log and continue
                        logger.debug(
                            "Could not parse push response as transcription: %s",
                            parse_ex,
                        )
        except Exception as ex:
            logger.error("Push loop terminated with unexpected error: %s", ex)
            fatal_ex = FoundryLocalException("Push loop terminated unexpectedly.")
            fatal_ex.__cause__ = ex
            self._output_queue.put(fatal_ex)
            self._output_queue.put(_SENTINEL)

    # --- Context manager support ---

    def __enter__(self) -> LiveAudioTranscriptionSession:
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        try:
            if self._started and not self._stopped:
                self.stop()
        except Exception as ex:
            logger.warning("Error during context manager cleanup: %s", ex)
