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

    def __init__(self, model_id: str, core_interop: CoreInterop):
        self._model_id = model_id
        self._core_interop = core_interop

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

    def start(self) -> None:
        """Start a real-time audio streaming session.

        Must be called before :meth:`append` or :meth:`get_transcription_stream`.
        Settings are frozen after this call.

        Raises:
            FoundryLocalException: If the session is already started or the
                native core returns an error.
        """
        with self._lock:
            if self._started:
                raise FoundryLocalException(
                    "Streaming session already started. Call stop() first."
                )

            # Freeze settings
            self._active_settings = self.settings.snapshot()

            self._output_queue = queue.Queue()
            self._push_queue = queue.Queue(maxsize=self._active_settings.push_queue_capacity)

            request = InteropRequest(params={
                "Model": self._model_id,
                "SampleRate": str(self._active_settings.sample_rate),
                "Channels": str(self._active_settings.channels),
                "BitsPerSample": str(self._active_settings.bits_per_sample),
            })

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

            # Start the push loop in a daemon thread
            self._push_thread = threading.Thread(
                target=self._push_loop, daemon=True
            )
            self._push_thread.start()

    def append(self, pcm_data: bytes) -> None:
        """Push a chunk of raw PCM audio data to the streaming session.

        Can be called from any thread (including audio device callbacks).
        Chunks are internally queued and serialized to the native core.

        The data is copied to avoid issues if the caller reuses the buffer.

        Args:
            pcm_data: Raw PCM audio bytes matching the configured format.

        Raises:
            FoundryLocalException: If no active streaming session exists.
        """
        if not self._started or self._stopped:
            raise FoundryLocalException(
                "No active streaming session. Call start() first."
            )

        # Copy the data to avoid issues if the caller reuses the buffer
        data_copy = bytes(pcm_data)
        self._push_queue.put(data_copy)

    def get_transcription_stream(self) -> Generator[LiveAudioTranscriptionResponse, None, None]:
        """Get the stream of transcription results.

        Results arrive as the native ASR engine processes audio data.
        The generator completes when :meth:`stop` is called and all
        remaining audio has been processed.

        Yields:
            Transcription results as ``LiveAudioTranscriptionResponse`` objects.

        Raises:
            FoundryLocalException: If no active streaming session exists,
                or if the push loop encountered a fatal error.
        """
        if self._output_queue is None:
            raise FoundryLocalException(
                "No active streaming session. Call start() first."
            )

        while True:
            item = self._output_queue.get()
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
        """
        with self._lock:
            if not self._started or self._stopped:
                return  # already stopped or never started

            self._stopped = True

        # 1. Signal push loop to finish (put sentinel)
        self._push_queue.put(_SENTINEL)

        # 2. Wait for push loop to finish draining
        if self._push_thread is not None:
            self._push_thread.join()

        # 3. Tell native core to flush and finalize
        request = InteropRequest(params={"SessionHandle": self._session_handle})
        response = self._core_interop.stop_audio_stream(request)

        # Parse final transcription from stop response
        if response.data:
            try:
                final_result = LiveAudioTranscriptionResponse.from_json(response.data)
                text = (final_result.content[0].text
                        if final_result.content else "")
                if text:
                    self._output_queue.put(final_result)
            except Exception as parse_ex:
                logger.debug("Could not parse stop response as transcription result: %s", parse_ex)

        # 4. Complete the output queue
        self._output_queue.put(_SENTINEL)

        # 5. Clean up
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

                request = InteropRequest(
                    params={"SessionHandle": self._session_handle}
                )
                response = self._core_interop.push_audio_data(request, audio_data)

                if response.error is not None:
                    error_info = CoreErrorResponse.try_parse(response.error)
                    code = error_info.code if error_info else "UNKNOWN"
                    fatal_ex = FoundryLocalException(
                        f"Push failed (code={code}): {response.error}"
                    )
                    logger.error("Terminating push loop due to push failure: %s",
                                 response.error)
                    self._output_queue.put(fatal_ex)
                    self._output_queue.put(_SENTINEL)
                    return

                # Parse transcription result from push response and surface it
                if response.data:
                    try:
                        transcription = LiveAudioTranscriptionResponse.from_json(
                            response.data
                        )
                        text = (transcription.content[0].text
                                if transcription.content else "")
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
            self._output_queue.put(
                FoundryLocalException("Push loop terminated unexpectedly.", ex)
            )
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
