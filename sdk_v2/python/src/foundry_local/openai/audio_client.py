# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""OpenAI-compatible audio transcription client backed by the Foundry Local native layer."""
from __future__ import annotations

import json
import queue
import threading
from dataclasses import dataclass
from typing import Any, Generator, List, Optional

# Sentinel placed on the stream queue by the background thread when transcription finishes.
_DONE = object()


class _StreamError:
    """Wraps an exception propagated from the background inference thread."""

    def __init__(self, exc: BaseException) -> None:
        self.exc = exc


class AudioSettings:
    """Settings supported by Foundry Local for audio transcription.

    Attributes:
        language: Language of the audio (e.g. ``"en"``).
        temperature: Sampling temperature (0.0 for deterministic results).
    """

    def __init__(
        self,
        language: Optional[str] = None,
        temperature: Optional[float] = None,
    ) -> None:
        self.language = language
        self.temperature = temperature


@dataclass
class AudioTranscriptionResponse:
    """Response from an audio transcription request.

    Attributes:
        text: The transcribed text.
    """

    text: str


class AudioClient:
    """OpenAI-compatible audio transcription client backed by Foundry Local Core.

    Each call creates a fresh native session (stateless — no session history).
    Supports non-streaming and streaming transcription of audio files.

    Attributes:
        model_id: The ID of the loaded Whisper model variant.
        settings: Tunable ``AudioSettings`` (language, temperature).
    """

    def __init__(self, model_id: str, model_ptr: Any) -> None:
        self.model_id = model_id
        self._model_ptr = model_ptr
        self.settings = AudioSettings()

    @staticmethod
    def _validate_audio_file_path(audio_file_path: str) -> None:
        """Validate that the audio file path is a non-empty string."""
        if not isinstance(audio_file_path, str) or not audio_file_path.strip():
            raise ValueError("Audio file path must be a non-empty string.")

    def _build_request_json(self, audio_file_path: str) -> str:
        """Build the JSON payload for audio transcription."""
        request: dict = {
            "Model": self.model_id,
            "FileName": audio_file_path,
        }

        # Metadata sub-dict mirrors the legacy format (lowercase keys, string values).
        metadata: dict[str, str] = {}

        if self.settings.language is not None:
            request["Language"] = self.settings.language
            metadata["language"] = self.settings.language

        if self.settings.temperature is not None:
            request["Temperature"] = self.settings.temperature
            metadata["temperature"] = str(self.settings.temperature)

        if metadata:
            request["metadata"] = metadata

        return json.dumps(request)

    def _run_native_request(self, request_json: str) -> str:
        """Create a fresh native session, process the request, return the response JSON string."""
        from foundry_local._native import ffi
        from foundry_local._native.api import api
        from foundry_local.items import Item, TextItem, TextItemType
        from foundry_local.request import Request

        session_out = ffi.new("flSession**")
        api.check_status(api.inference.Session_Create(self._model_ptr, session_out))
        session_ptr = session_out[0]

        try:
            req = Request()
            text_item = TextItem(request_json, TextItemType.OPENAI_JSON)
            req.add_item(text_item)  # transfers ownership of text_item

            resp_out = ffi.new("flResponse**")
            api.check_status(api.inference.Session_ProcessRequest(session_ptr, req._ptr, resp_out))
            resp_ptr = resp_out[0]

            try:
                item_out = ffi.new("flItem**")
                api.check_status(api.inference.Response_GetItem(resp_ptr, 0, item_out))
                # owns=False — response owns the item; text is copied to a Python str.
                response_item = Item.from_native(item_out[0], owns=False)
                return response_item.text
            finally:
                api.inference.Response_Release(resp_ptr)
        finally:
            api.inference.Session_Release(session_ptr)

    def transcribe(self, audio_file_path: str) -> AudioTranscriptionResponse:
        """Transcribe an audio file (non-streaming).

        Args:
            audio_file_path: Path to the audio file to transcribe.

        Returns:
            An ``AudioTranscriptionResponse`` containing the transcribed text.

        Raises:
            ValueError: If *audio_file_path* is not a non-empty string.
            FoundryLocalException: If the native transcription call fails.
        """
        self._validate_audio_file_path(audio_file_path)

        request_json = self._build_request_json(audio_file_path)
        response_json = self._run_native_request(request_json)
        data = json.loads(response_json)
        return AudioTranscriptionResponse(text=data.get("text", ""))

    def transcribe_streaming(self, audio_file_path: str) -> Generator[AudioTranscriptionResponse, None, None]:
        """Transcribe an audio file with streaming chunks.

        Consume with a standard ``for`` loop::

            for chunk in audio_client.transcribe_streaming("recording.mp3"):
                print(chunk.text, end="", flush=True)

        Args:
            audio_file_path: Path to the audio file to transcribe.

        Returns:
            A generator of ``AudioTranscriptionResponse`` objects.

        Raises:
            ValueError: If *audio_file_path* is not a non-empty string.
            FoundryLocalException: If the native layer returns an error.
        """
        self._validate_audio_file_path(audio_file_path)

        request_json = self._build_request_json(audio_file_path)

        from foundry_local._native import ffi
        from foundry_local._native.api import api
        from foundry_local.items import Item, TextItem, TextItemType
        from foundry_local.request import Request

        q: queue.Queue = queue.Queue()

        # Shared slot so the generator can cancel the in-flight request on early exit.
        req_ref: List[Optional[Any]] = [None]

        def _on_native_cb(data: Any, user_data: Any) -> int:
            try:
                if data.item_queue != ffi.NULL:
                    item_out = ffi.new("flItem**")
                    while api.item.ItemQueue_TryPop(data.item_queue, item_out):
                        q.put(Item.from_native(item_out[0], owns=True))
            except Exception as exc:
                q.put(_StreamError(exc))
                return 1
            return 0

        # Create the cffi callback in this scope so it outlives the background thread.
        _cb_ref = ffi.callback("int(flStreamingCallbackData, void *)", _on_native_cb)

        def _run() -> None:
            session_ptr = None
            try:
                session_out = ffi.new("flSession**")
                api.check_status(api.inference.Session_Create(self._model_ptr, session_out))
                session_ptr = session_out[0]

                api.check_status(
                    api.inference.Session_SetStreamingCallback(session_ptr, _cb_ref, ffi.NULL)
                )

                req = Request()
                req_ref[0] = req
                text_item = TextItem(request_json, TextItemType.OPENAI_JSON)
                req.add_item(text_item)

                resp_out = ffi.new("flResponse**")
                api.check_status(
                    api.inference.Session_ProcessRequest(session_ptr, req._ptr, resp_out)
                )
                api.inference.Response_Release(resp_out[0])
            except Exception as exc:
                q.put(_StreamError(exc))
            finally:
                if session_ptr is not None:
                    api.inference.Session_Release(session_ptr)
                q.put(_DONE)

        t = threading.Thread(target=_run, daemon=True)
        t.start()

        completed = False
        try:
            while True:
                queue_item = q.get()

                if queue_item is _DONE:
                    completed = True
                    break

                if isinstance(queue_item, _StreamError):
                    raise queue_item.exc

                data = json.loads(queue_item.text)
                yield AudioTranscriptionResponse(text=data.get("text", ""))
        finally:
            if not completed:
                if req_ref[0] is not None:
                    req_ref[0].cancel()
            t.join()
            _ = _cb_ref
