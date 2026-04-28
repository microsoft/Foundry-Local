# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
from __future__ import annotations

import asyncio
import json
import logging
import requests

from typing import List
from urllib.parse import quote

from ..exception import FoundryLocalException
from ..version import __version__ as sdk_version
from .core_interop import CoreInterop, InteropRequest

logger = logging.getLogger(__name__)


class ModelLoadManager:
    """Manages loading and unloading of models in Foundry Local.

    Can operate in two modes: direct interop with Foundry Local Core, or via
    an external web service if the configuration provides a
    ``WebServiceExternalUrl`` value.
    """

    _headers = {"user-agent": f"foundry-local-python-sdk/{sdk_version}"}

    def __init__(self, core_interop: CoreInterop, external_service_url: str = None):
        self._core_interop = core_interop
        self._external_service_url = external_service_url

    async def load(self, model_id: str) -> None:
        """
        Load a model by its ID.

        This method loads a model either via direct interop with Foundry Local Core
        or, if an external service URL is configured, by calling the external web
        service.

        :param model_id: The ID of the model to load.
        :raises FoundryLocalException: If the model cannot be loaded successfully,
            for example due to an error returned from Foundry Local Core or from
            the external service, including underlying HTTP or network errors when
            communicating with the external service.
        """
        if self._external_service_url:
            await self._web_load_model(model_id)
            return

        request = InteropRequest({"Model": model_id})
        response = await self._core_interop.execute_command("load_model", request)
        if response.error is not None:
            raise FoundryLocalException(f"Failed to load model {model_id}: {response.error}")

    async def unload(self, model_id: str) -> None:
        """
        Unload a model by its ID.
        :param model_id: The ID of the model to unload.
        """
        if self._external_service_url:
            await self._web_unload_model(model_id)
            return
    
        request = InteropRequest({"Model": model_id})
        response = await self._core_interop.execute_command("unload_model", request)
        if response.error is not None:
            raise FoundryLocalException(f"Failed to unload model {model_id}: {response.error}")

    async def list_loaded(self) -> list[str]:
        """
        List loaded models.
        :return: List of loaded model IDs
        """
        if self._external_service_url:
            return await self._web_list_loaded_models()

        response = await self._core_interop.execute_command("list_loaded_models")
        if response.error is not None:
            raise FoundryLocalException(f"Failed to list loaded models: {response.error}")

        try:
            model_ids = json.loads(response.data)
        except json.JSONDecodeError as e:
            raise FoundryLocalException(f"Failed to decode JSON response: Response was: {response.data}") from e

        return model_ids

    async def _web_list_loaded_models(self) -> List[str]:
        try:
            response = await asyncio.to_thread(
                requests.get, f"{self._external_service_url}/models/loaded",
                headers=self._headers, timeout=10
            )

            if not response.ok:
                raise FoundryLocalException(
                    f"Error listing loaded models from {self._external_service_url}: {response.reason}"
                )

            content = response.text
            logger.debug("Loaded models json from %s: %s", self._external_service_url, content)

            model_list = json.loads(content)
            return model_list if model_list is not None else []
        except requests.RequestException as e:
            raise FoundryLocalException(
                f"HTTP request failed when listing loaded models from {self._external_service_url}"
            ) from e
        except json.JSONDecodeError as e:
            raise FoundryLocalException(f"Failed to decode JSON response: Response was: {content}") from e

    async def _web_load_model(self, model_id: str) -> None:
        """
        Load a model via the external web service.
 
        :param model_id: The ID of the model to load
        :raises FoundryLocalException: If the HTTP request fails or response is invalid
        """
        try:
            encoded_model_id = quote(model_id)
            url = f"{self._external_service_url}/models/load/{encoded_model_id}"

            # Future: add query params like load timeout
            # query_params = {
            #     # "timeout": "30"
            # }
            # response = requests.get(url, params=query_params)

            response = await asyncio.to_thread(requests.get, url, headers=self._headers, timeout=10)

            if not response.ok:
                raise FoundryLocalException(
                    f"Error loading model {model_id} from {self._external_service_url}: "
                    f"{response.reason}"
                )

            content = response.text
            logger.info("Model %s loaded successfully from %s: %s",
                        model_id, self._external_service_url, content)

        except requests.RequestException as e:
            raise FoundryLocalException(
                f"HTTP request failed when loading model {model_id} from {self._external_service_url}: {e}"
            ) from e

    async def _web_unload_model(self, model_id: str) -> None:
        try:
            encoded_model_id = quote(model_id)
            url = f"{self._external_service_url}/models/unload/{encoded_model_id}"

            response = await asyncio.to_thread(requests.get, url, headers=self._headers, timeout=10)

            if not response.ok:
                raise FoundryLocalException(
                    f"Error unloading model {model_id} from {self._external_service_url}: "
                    f"{response.reason}"
                )

            content = response.text
            logger.info("Model %s unloaded successfully from %s: %s",
                        model_id, self._external_service_url, content)

        except requests.RequestException as e:
            raise FoundryLocalException(
                f"HTTP request failed when unloading model {model_id} from {self._external_service_url}: {e}"
            ) from e
