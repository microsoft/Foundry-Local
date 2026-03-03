# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
from __future__ import annotations

import logging
from typing import Callable, List, Optional

from .imodel import IModel
from .openai.chat_client import ChatClient
from .openai.audio_client import AudioClient
from .model_variant import ModelVariant
from .exception import FoundryLocalException
from .detail.core_interop import CoreInterop
from .detail.utils import get_cached_model_ids

logger = logging.getLogger(__name__)


class Model(IModel):
    """A model identified by an alias that groups one or more ``ModelVariant`` instances.

    Operations are delegated to the currently selected variant.
    """

    def __init__(self, model_variant: ModelVariant, core_interop: CoreInterop):
        self._alias = model_variant.alias
        self._variants: List[ModelVariant] = [model_variant]
        # Variants are sorted by Core, so the first one added is the default
        self._selected_variant = model_variant
        self._core_interop = core_interop

    def _add_variant(self, variant: ModelVariant) -> None:
        if variant.alias != self._alias:
            raise FoundryLocalException(
                f"Variant alias {variant.alias} does not match model alias {self._alias}"
            )

        self._variants.append(variant)

        # Prefer the highest priority locally cached variant
        if variant.info.cached and not self._selected_variant.info.cached:
            self._selected_variant = variant

    def select_variant(self, variant: ModelVariant) -> None:
        """
        Select a specific model variant by its ModelVariant object.
        The selected variant will be used for IModel operations.
        
        :param variant: ModelVariant to select
        :raises FoundryLocalException: If variant is not valid for this model
        """
        if variant not in self._variants:
            raise FoundryLocalException(
                f"Model {self._alias} does not have a {variant.id} variant."
            )

        self._selected_variant = variant

    def get_latest_version(self, variant: ModelVariant) -> ModelVariant:
        """
        Get the latest version of the specified model variant.
        
        :param variant: Model variant
        :return: ModelVariant for latest version. Same as variant if that is the latest version
        :raises FoundryLocalException: If variant is not valid for this model
        """
        # Variants are sorted by version, so the first one matching the name is the latest version
        for v in self._variants:
            if v.info.name == variant.info.name:
                return v
        
        raise FoundryLocalException(
            f"Model {self._alias} does not have a {variant.id} variant."
        )

    @property
    def variants(self) -> List[ModelVariant]:
        """List of all variants for this model."""
        return self._variants.copy()  # Return a copy to prevent external modification

    @property
    def selected_variant(self) -> ModelVariant:
        """Currently selected variant."""
        return self._selected_variant

    @property
    def id(self) -> str:
        """Model Id of the currently selected variant."""
        return self._selected_variant.id

    @property
    def alias(self) -> str:
        """Alias of this model."""
        return self._alias

    @property
    def is_cached(self) -> bool:
        """Is the currently selected variant cached locally?"""
        cached_models = get_cached_model_ids(self._core_interop)
        return self.id in cached_models

    @property
    def is_loaded(self) -> bool:
        """Is the currently selected variant loaded in memory?"""
        return self._selected_variant.is_loaded

    def download(self, progress_callback: Optional[Callable[[float], None]] = None) -> None:
        """Download the currently selected variant."""
        self._selected_variant.download(progress_callback)

    def get_path(self) -> str:
        """Get the path to the currently selected variant."""
        return self._selected_variant.get_path()

    def load(self) -> None:
        """Load the currently selected variant into memory."""
        self._selected_variant.load()

    def unload(self) -> None:
        """Unload the currently selected variant from memory."""
        self._selected_variant.unload()

    def remove_from_cache(self) -> None:
        """Remove the currently selected variant from the local cache."""
        self._selected_variant.remove_from_cache()

    def get_chat_client(self) -> ChatClient:
        """Get a chat client for the currently selected variant."""
        return self._selected_variant.get_chat_client()
    
    def get_audio_client(self) -> AudioClient:
        """Get an audio client for the currently selected variant."""
        return self._selected_variant.get_audio_client()
