# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
from __future__ import annotations

from abc import ABC, abstractmethod
from typing import Callable, List, Optional

from .openai.chat_client import ChatClient
from .openai.audio_client import AudioClient
from .openai.embedding_client import EmbeddingClient
from .openai.responses_client import ResponsesClient
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

    @abstractmethod
    def get_embedding_client(self) -> 'EmbeddingClient':
        """
        Get an OpenAI API based EmbeddingClient.
        :return: EmbeddingClient instance.
        """
        pass

    @abstractmethod
    def create_responses_client(self, base_url: str) -> 'ResponsesClient':
        """
        Create an OpenAI Responses API client bound to the running web service.

        Unlike the other clients, the Responses API is HTTP-only and requires
        the Foundry Local web service to be started. Pass the base URL
        returned by :attr:`FoundryLocalManager.urls` (e.g. ``manager.urls[0]``),
        or use :meth:`FoundryLocalManager.create_responses_client` directly.

        :param base_url: Base URL of the running Foundry Local web service.
        :return: ResponsesClient instance for this variant's model id.
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
