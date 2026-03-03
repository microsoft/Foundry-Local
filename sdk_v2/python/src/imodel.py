# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
from __future__ import annotations

from abc import ABC, abstractmethod
from typing import Callable, Optional

from src.openai.chat_client import ChatClient
from src.openai.audio_client import AudioClient

class IModel(ABC):
    """Abstract interface for a model that can be downloaded, loaded, and used for inference."""

    @property
    @abstractmethod
    def id(self) -> str:
        """Unique model id."""
        pass

    @property
    @abstractmethod
    def alias(self) -> str:
        """Model alias."""
        pass

    @property
    @abstractmethod
    def is_cached(self) -> bool:
        """True if the model is present in the local cache."""
        pass

    @property
    @abstractmethod
    def is_loaded(self) -> bool:
        """True if the model is loaded into memory."""
        pass

    @abstractmethod
    def download(self, progress_callback: Callable[[float], None] = None) -> None:
        """
        Download the model to local cache if not already present.
        :param progress_callback: Optional callback function for download progress as a percentage (0.0 to 100.0).
        """
        pass

    @abstractmethod
    def get_path(self) -> str:
        """
        Gets the model path if cached.
        :return: Path of model directory.
        """
        pass

    @abstractmethod
    def load(self,) -> None:
        """
        Load the model into memory if not already loaded.
        """
        pass

    @abstractmethod
    def remove_from_cache(self) -> None:
        """
        Remove the model from the local cache.
        """
        pass

    @abstractmethod
    def unload(self) -> None:
        """
        Unload the model if loaded.
        """
        pass

    @abstractmethod
    def get_chat_client(self) -> ChatClient:
        """
        Get an OpenAI API based ChatClient.
        :return: ChatClient instance.
        """
        pass

    @abstractmethod
    def get_audio_client(self) -> AudioClient:
        """
        Get an OpenAI API based AudioClient.
        :return: AudioClient instance.
        """
        pass
