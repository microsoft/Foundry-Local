# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
from __future__ import annotations

from abc import ABC, abstractmethod
from threading import Event
from typing import Callable, List, Optional

from .openai.chat_client import ChatClient
from .openai.audio_client import AudioClient
from .detail.model_data_types import ModelInfo

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
    def info(self) -> ModelInfo:
        """Full model metadata."""
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

    @property
    @abstractmethod
    def context_length(self) -> Optional[int]:
        """Maximum context length (in tokens) supported by the model, or ``None`` if unknown."""
        pass

    @property
    @abstractmethod
    def input_modalities(self) -> Optional[str]:
        """Comma-separated input modalities (e.g. ``"text,image"``), or ``None`` if unknown."""
        pass

    @property
    @abstractmethod
    def output_modalities(self) -> Optional[str]:
        """Comma-separated output modalities (e.g. ``"text"``), or ``None`` if unknown."""
        pass

    @property
    @abstractmethod
    def capabilities(self) -> Optional[str]:
        """Comma-separated capability tags (e.g. ``"chat,completion"``), or ``None`` if unknown."""
        pass

    @property
    @abstractmethod
    def supports_tool_calling(self) -> Optional[bool]:
        """Whether the model supports tool/function calling, or ``None`` if unknown."""
        pass

    @abstractmethod
    def download(self, progress_callback: Callable[[float], None] = None,
                 cancel_event: Optional[Event] = None) -> None:
        """
        Download the model to local cache if not already present.
        :param progress_callback: Optional callback function for download progress as a percentage (0.0 to 100.0).
        :param cancel_event: Optional ``threading.Event``. When set, the download will be
            cancelled at the next progress update and ``FoundryLocalException`` is raised.
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
    def load(self) -> None:
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

    @property
    @abstractmethod
    def variants(self) -> List['IModel']:
        """Variants of the model that are available. Variants of the model are optimized for different devices."""
        pass

    @abstractmethod
    def select_variant(self, variant: 'IModel') -> None:
        """
        Select a model variant from ``variants`` to use for IModel operations.
        An IModel from ``variants`` can also be used directly.

        :param variant: Model variant to select. Must be one of the variants in ``variants``.
        :raises FoundryLocalException: If variant is not valid for this model.
        """
        pass
