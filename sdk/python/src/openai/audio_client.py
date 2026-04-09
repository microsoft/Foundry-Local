# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------

from __future__ import annotations

import json
import logging
import queue
import threading
from dataclasses import dataclass
from typing import Generator, List, Optional

from ..detail.core_interop import CoreInterop, InteropRequest
from ..exception import FoundryLocalException

logger = logging.getLogger(__name__)


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
    ):
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

    Supports non-streaming and streaming transcription of audio files.

    Attributes:
        model_id: The ID of the loaded Whisper model variant.
        settings: Tunable ``AudioSettings`` (language, temperature).
    """

    def __init__(self, model_id: str, core_interop: CoreInterop):
        self.model_id = model_id
        self.settings = AudioSettings()
        self._core_interop = core_interop

    @staticmethod
    def _validate_audio_file_path(audio_file_path: str) -> None:
        """Validate that the audio file path is a non-empty string."""
        if not isinstance(audio_file_path, str) or audio_file_path.strip() == "":
            raise ValueError("Audio file path must be a non-empty string.")

    def _create_request_json(self, audio_file_path: str) -> str:
        """Build the JSON payload for the ``audio_transcribe`` native command."""
        request: dict = {
            "Model": self.model_id,
            "FileName": audio_file_path,
        }

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

    def transcribe(self, audio_file_path: str) -> AudioTranscriptionResponse:
        """Transcribe an audio file (non-streaming).

        Args:
            audio_file_path: Path to the audio file to transcribe.

        Returns:
            An ``AudioTranscriptionResponse`` containing the transcribed text.

        Raises:
            ValueError: If *audio_file_path* is not a non-empty string.
            FoundryLocalException: If the underlying native transcription command fails.
        """
        self._validate_audio_file_path(audio_file_path)

        request_json = self._create_request_json(audio_file_path)
        request = InteropRequest(params={"OpenAICreateRequest": request_json})

        response = self._core_interop.execute_command("audio_transcribe", request)
        if response.error is not None:
            raise FoundryLocalException(
                f"Audio transcription failed for model '{self.model_id}': {response.error}"
            )

        data = json.loads(response.data)
        return AudioTranscriptionResponse(text=data.get("text", ""))

    def _stream_chunks(self, request_json: str) -> Generator[AudioTranscriptionResponse, None, None]:
        """Background-thread generator that yields parsed chunks from the native streaming call."""
        _SENTINEL = object()
        chunk_queue: queue.Queue = queue.Queue()
        errors: List[Exception] = []

        def _on_chunk(chunk_str: str) -> None:
            chunk_data = json.loads(chunk_str)
            chunk_queue.put(AudioTranscriptionResponse(text=chunk_data.get("text", "")))

        def _run() -> None:
            try:
                resp = self._core_interop.execute_command_with_callback(
                    "audio_transcribe",
                    InteropRequest(params={"OpenAICreateRequest": request_json}),
                    _on_chunk,
                )
                if resp.error is not None:
                    errors.append(
                        FoundryLocalException(
                            f"Streaming audio transcription failed for model '{self.model_id}': {resp.error}"
                        )
                    )
            except Exception as exc:
                errors.append(exc)
            finally:
                chunk_queue.put(_SENTINEL)

        threading.Thread(target=_run, daemon=True).start()
        while (item := chunk_queue.get()) is not _SENTINEL:
            yield item
        if errors:
            raise errors[0]

    def transcribe_streaming(
        self,
        audio_file_path: str,
    ) -> Generator[AudioTranscriptionResponse, None, None]:
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
            FoundryLocalException: If the underlying native transcription command fails.
        """
        self._validate_audio_file_path(audio_file_path)

        request_json = self._create_request_json(audio_file_path)
        return self._stream_chunks(request_json)