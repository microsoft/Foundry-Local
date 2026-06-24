# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""OpenAI-compatible audio transcription client backed by the Foundry Local native layer."""
from __future__ import annotations

import asyncio
import json
from dataclasses import dataclass
from typing import TYPE_CHECKING, AsyncGenerator

if TYPE_CHECKING:
    from foundry_local_sdk.imodel import IModel
    from foundry_local_sdk.openai.live_audio_session import LiveAudioTranscriptionSession


class AudioSettings:
    """Settings supported by Foundry Local for audio transcription.

    Attributes:
        language: Language of the audio (e.g. ``"en"``).
        temperature: Sampling temperature (0.0 for deterministic results).
    """

    def __init__(
        self,
        language: str | None = None,
        temperature: float | None = None,
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

    def __init__(self, model_id: str, model: IModel) -> None:
        self.model_id = model_id
        # Hold the IModel reference so the underlying native model pointer
        # cannot be released out from under us.
        self._model = model
        self.settings = AudioSettings()

    def create_live_transcription_session(self) -> "LiveAudioTranscriptionSession":
        """Create a real-time streaming transcription session.

        Audio data is pushed in as PCM chunks via :meth:`LiveAudioTranscriptionSession.append` and
        transcription results are returned as a synchronous iterator via
        :meth:`LiveAudioTranscriptionSession.get_stream`.

        Returns:
            A streaming session that should be stopped when done. Supports use as a context manager::

                with audio_client.create_live_transcription_session() as session:
                    session.settings.sample_rate = 16000
                    session.start()
                    session.append(pcm_bytes)
                    for result in session.get_stream():
                        print(result.content[0].text)
        """
        from foundry_local_sdk.openai.live_audio_session import LiveAudioTranscriptionSession

        return LiveAudioTranscriptionSession(self.model_id, self._model)

    @staticmethod
    def _validate_audio_file_path(audio_file_path: str) -> None:
        """Validate that the audio file path is a non-empty string."""
        if not isinstance(audio_file_path, str) or not audio_file_path.strip():
            raise ValueError("Audio file path must be a non-empty string.")

    def _build_request_json(self, audio_file_path: str) -> str:
        """Build the JSON payload for audio transcription.

        Flat object with lowercase keys consumed by the native ``AudioSession``:
        ``model``, ``filename``, optional ``language``, optional ``temperature``.
        """
        request: dict = {
            "model": self.model_id,
            "filename": audio_file_path,
        }

        if self.settings.language is not None:
            request["language"] = self.settings.language

        if self.settings.temperature is not None:
            request["temperature"] = self.settings.temperature

        return json.dumps(request)

    async def _run_native_request(self, request_json: str) -> str:
        """Create a fresh AudioSession, process the request, return the response JSON string."""
        from foundry_local_sdk.items import TextItem, TextItemType
        from foundry_local_sdk.request import Request
        from foundry_local_sdk.session import AudioSession

        def _blocking():
            with AudioSession(self._model) as session:
                with Request() as request:
                    request.add_item(TextItem(request_json, TextItemType.OPENAI_JSON))
                    response = session.process_request(request)
                    try:
                        return response.get_item(0).text
                    finally:
                        response._close()
        
        return await asyncio.to_thread(_blocking)

    async def transcribe(self, audio_file_path: str) -> AudioTranscriptionResponse:
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
        response_json = await self._run_native_request(request_json)
        data = json.loads(response_json)
        return AudioTranscriptionResponse(text=data.get("text", ""))

    async def transcribe_streaming(self, audio_file_path: str) -> AsyncGenerator[AudioTranscriptionResponse, None]:
        """Transcribe an audio file with streaming chunks.

        Consume with an async ``for`` loop::

            async for chunk in audio_client.transcribe_streaming("recording.mp3"):
                print(chunk.text, end="", flush=True)

        Args:
            audio_file_path: Path to the audio file to transcribe.

        Yields:
            ``AudioTranscriptionResponse`` objects as they arrive.

        Raises:
            ValueError: If *audio_file_path* is not a non-empty string.
            FoundryLocalException: If the native layer returns an error.
        """
        self._validate_audio_file_path(audio_file_path)
        request_json = self._build_request_json(audio_file_path)
        async for item in self._transcribe_streaming_impl(request_json):
            yield item

    async def _transcribe_streaming_impl(self, request_json: str) -> AsyncGenerator[AudioTranscriptionResponse, None]:
        from foundry_local_sdk.items import TextItem, TextItemType
        from foundry_local_sdk.request import Request
        from foundry_local_sdk.session import AudioSession

        def _blocking_stream():
            """Run blocking streaming in a separate thread."""
            items = []
            with AudioSession(self._model) as session:
                session.set_streaming(True)
                with Request() as request:
                    request.add_item(TextItem(request_json, TextItemType.OPENAI_JSON))
                    with session.process_streaming_request(request) as stream:
                        for item in stream:
                            items.append(item)
            return items

        # Run blocking operation in thread and yield each result
        items = await asyncio.to_thread(_blocking_stream)
        for item in items:
            data = json.loads(item.text)
            yield AudioTranscriptionResponse(text=data.get("text", ""))
