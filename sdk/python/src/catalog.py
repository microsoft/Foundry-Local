# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------

from __future__ import annotations

import datetime
import logging
import threading
from typing import List, Optional
from pydantic import TypeAdapter

from .imodel import IModel
from .detail.model import Model
from .detail.model_variant import ModelVariant

from .detail.core_interop import CoreInterop, get_cached_model_ids
from .detail.model_data_types import ModelInfo
from .detail.model_load_manager import ModelLoadManager
from .exception import FoundryLocalException

logger = logging.getLogger(__name__)

class Catalog():
    """Model catalog for discovering and querying available models.

    Provides methods to list models, look up by alias or ID, and query
    cached or loaded models. The model list is refreshed every 6 hours.
    """

    def __init__(self, model_load_manager: ModelLoadManager, core_interop: CoreInterop):
        """Initialize the Catalog.

        Args:
            model_load_manager: Manager for loading/unloading models.
            core_interop: Native interop layer for Foundry Local Core.
        """
        self._core_interop = core_interop
        self._model_load_manager = model_load_manager
        self._lock = threading.Lock()

        self._models: List[ModelInfo] = []
        self._model_alias_to_model = {}
        self._model_id_to_model_variant = {}
        self._last_fetch = datetime.datetime.min

        response = core_interop.execute_command("get_catalog_name")
        if response.error is not None:
            raise FoundryLocalException(f"Failed to get catalog name: {response.error}")

        self.name = response.data

    def _update_models(self):
        with self._lock:
            # refresh every 6 hours
            if (datetime.datetime.now() - self._last_fetch) < datetime.timedelta(hours=6):
                return

            response = self._core_interop.execute_command("get_model_list")
            if response.error is not None:
                raise FoundryLocalException(f"Failed to get model list: {response.error}")

            model_list_json = response.data

            adapter = TypeAdapter(list[ModelInfo])
            models: List[ModelInfo] = adapter.validate_json(model_list_json)

            self._model_alias_to_model.clear()
            self._model_id_to_model_variant.clear()

            for model_info in models:
                variant = ModelVariant(model_info, self._model_load_manager, self._core_interop)

                value = self._model_alias_to_model.get(model_info.alias)
                if value is None:
                    value = Model(variant, self._core_interop)
                    self._model_alias_to_model[model_info.alias] = value
                else:
                    value._add_variant(variant)

                self._model_id_to_model_variant[variant.id] = variant

            self._last_fetch = datetime.datetime.now()
            self._models = models

    def list_models(self) -> List[IModel]:
        """
        List the available models in the catalog.
        :return: List of IModel instances.
        """
        self._update_models()
        return list(self._model_alias_to_model.values())

    def get_model(self, model_alias: str) -> Optional[IModel]:
        """
        Lookup a model by its alias.
        :param model_alias: Model alias.
        :return: IModel if found.
        """
        self._update_models()
        return self._model_alias_to_model.get(model_alias)

    def get_model_variant(self, model_id: str) -> Optional[IModel]:
        """
        Lookup a model variant by its unique model id.
        NOTE: This will return an IModel with a single variant. Use get_model to get an IModel with all available
        variants.
        :param model_id: Model id.
        :return: IModel if found.
        """
        self._update_models()
        return self._model_id_to_model_variant.get(model_id)

    def get_cached_models(self) -> List[IModel]:
        """
        Get a list of currently downloaded models from the model cache.
        :return: List of IModel instances.
        """
        self._update_models()

        cached_model_ids = get_cached_model_ids(self._core_interop)

        cached_models: List[IModel] = []
        for model_id in cached_model_ids:
            model_variant = self._model_id_to_model_variant.get(model_id)
            if model_variant is not None:
                cached_models.append(model_variant)

        return cached_models

    def get_loaded_models(self) -> List[IModel]:
        """
        Get a list of the currently loaded models.
        :return: List of IModel instances.
        """
        self._update_models()

        loaded_model_ids = self._model_load_manager.list_loaded()
        loaded_models: List[IModel] = []
        
        for model_id in loaded_model_ids:
            model_variant = self._model_id_to_model_variant.get(model_id)
            if model_variant is not None:
                loaded_models.append(model_variant)
        
        return loaded_models