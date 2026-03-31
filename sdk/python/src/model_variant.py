# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
from __future__ import annotations

import logging
from typing import Callable, Optional

from .imodel import IModel
from .exception import FoundryLocalException

from .detail.core_interop import CoreInterop, InteropRequest
from .detail.model_data_types import ModelInfo
from .detail.core_interop import get_cached_model_ids
from .detail.model_load_manager import ModelLoadManager
from .openai.audio_client import AudioClient
from .openai.chat_client import ChatClient

logger = logging.getLogger(__name__)


class ModelVariant(IModel):
    """A specific variant of a model (e.g. a particular device type, version, or quantization).

    Implements ``IModel`` and provides download, cache, load/unload, and
    client-creation operations for a single model variant.
    """

    def __init__(self, model_info: ModelInfo, model_load_manager: ModelLoadManager, core_interop: CoreInterop):
        """Initialize a ModelVariant.

        Args:
            model_info: Catalog metadata for this variant.
            model_load_manager: Manager for loading/unloading models.
            core_interop: Native interop layer for Foundry Local Core.
        """
        self._model_info = model_info
        self._model_load_manager = model_load_manager
        self._core_interop = core_interop

        self._id = model_info.id
        self._alias = model_info.alias

    @property
    def id(self) -> str:
        """Unique model variant ID (e.g. ``name:version``)."""
        return self._id

    @property
    def alias(self) -> str:
        """Model alias shared across variants."""
        return self._alias

    @property
    def info(self) -> ModelInfo:
        """Full catalog metadata for this variant."""
        return self._model_info

    @property
    def context_length(self) -> Optional[int]:
        """Maximum context length (in tokens) supported by this variant, or ``None`` if unknown."""
        return self._model_info.context_length

    @property
    def input_modalities(self) -> Optional[str]:
        """Comma-separated input modalities (e.g. ``"text,image"``), or ``None`` if unknown."""
        return self._model_info.input_modalities

    @property
    def output_modalities(self) -> Optional[str]:
        """Comma-separated output modalities (e.g. ``"text"``), or ``None`` if unknown."""
        return self._model_info.output_modalities

    @property
    def capabilities(self) -> Optional[str]:
        """Comma-separated capability tags (e.g. ``"chat,completion"``), or ``None`` if unknown."""
        return self._model_info.capabilities

    @property
    def supports_tool_calling(self) -> Optional[bool]:
        """Whether this variant supports tool/function calling, or ``None`` if unknown."""
        return self._model_info.supports_tool_calling

    @property
    def is_cached(self) -> bool:
        """``True`` if this variant is present in the local model cache."""
        cached_model_ids = get_cached_model_ids(self._core_interop)
        return self.id in cached_model_ids

    @property
    def is_loaded(self) -> bool:
        """``True`` if this variant is currently loaded into memory."""
        loaded_model_ids = self._model_load_manager.list_loaded()
        return self.id in loaded_model_ids

    def download(self, progress_callback: Callable[[float], None] = None):
        """Download this variant to the local cache.

        Args:
            progress_callback: Optional callback receiving download progress as a
                percentage (0.0 to 100.0).
        """
        request = InteropRequest(params={"Model": self.id})
        if progress_callback is None:
            response = self._core_interop.execute_command("download_model", request)
        else:
            response = self._core_interop.execute_command_with_callback(
                "download_model", request,
                lambda pct_str: progress_callback(float(pct_str))
            )

        logger.info("Download response: %s", response)
        if response.error is not None:
            raise FoundryLocalException(f"Failed to download model: {response.error}")

    def get_path(self) -> str:
        """Get the local file-system path to this variant if cached.

        Returns:
            Path to the model directory.

        Raises:
            FoundryLocalException: If the model path cannot be retrieved.
        """
        request = InteropRequest(params={"Model": self.id})
        response = self._core_interop.execute_command("get_model_path", request)
        if response.error is not None:
            raise FoundryLocalException(f"Failed to get model path: {response.error}")

        return response.data

    def load(self) -> None:
        """Load this variant into memory for inference."""
        self._model_load_manager.load(self.id)

    def remove_from_cache(self) -> None:
        """Remove this variant from the local model cache."""
        request = InteropRequest(params={"Model": self.id})
        response = self._core_interop.execute_command("remove_cached_model", request)
        if response.error is not None:
            raise FoundryLocalException(f"Failed to remove model from cache: {response.error}")


    def unload(self) -> None:
        """Unload this variant from memory."""
        self._model_load_manager.unload(self.id)

    def get_chat_client(self) -> ChatClient:
        """Create an OpenAI-compatible ``ChatClient`` for this variant."""
        return ChatClient(self.id, self._core_interop)

    def get_audio_client(self) -> AudioClient:
        """Create an OpenAI-compatible ``AudioClient`` for this variant."""
        return AudioClient(self.id, self._core_interop)