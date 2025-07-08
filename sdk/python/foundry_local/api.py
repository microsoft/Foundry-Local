# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
from __future__ import annotations

import logging
import os
import platform

from httpx import Timeout

from foundry_local.client import HttpResponseError, HttpxClient
from foundry_local.models import ExecutionProvider, FoundryModelInfo
from foundry_local.service import assert_foundry_installed, get_service_uri, start_service

logger = logging.getLogger(__name__)


class FoundryLocalManager:
    """Manager for Foundry Local SDK operations."""

    def __init__(
        self, alias_or_model_id: str | None = None, bootstrap: bool = True, timeout: float | Timeout | None = None
    ):
        """
        Initialize the Foundry Local SDK.

        Args:
            alias_or_model_id (str | None): Alias or Model ID to download and load. Only used if bootstrap is True.
            bootstrap (bool): If True, start the service if it is not running.
            timeout (float | Timeout | None): Timeout for the HTTP client. Default is None.
        """
        assert_foundry_installed()
        self._timeout = timeout
        self._service_uri = None
        self._httpx_client = None
        self._set_service_uri_and_client(get_service_uri())
        self._catalog_list = None
        self._catalog_dict = None
        if bootstrap:
            self.start_service()
            if alias_or_model_id is not None:
                self.download_model(alias_or_model_id)
                self.load_model(alias_or_model_id)

    def _set_service_uri_and_client(self, service_uri: str | None):
        """
        Set the service URI and HTTP client.

        Args:
            service_uri (str | None): URI of the Foundry service.
        """
        self._service_uri = service_uri
        self._httpx_client = HttpxClient(service_uri, timeout=self._timeout) if service_uri else None

    @property
    def service_uri(self) -> str:
        """
        Get the service URI.

        Returns:
            str: URI of the Foundry service.

        Raises:
            RuntimeError: If the service URI is not set.
        """
        if self._service_uri is None:
            raise RuntimeError("Service URI is not set. Please start the service first.")
        return self._service_uri

    @property
    def httpx_client(self) -> HttpxClient:
        """
        Get the HTTP client.

        Returns:
            HttpxClient: HTTP client instance.

        Raises:
            RuntimeError: If the HTTP client is not set.
        """
        if self._httpx_client is None:
            raise RuntimeError("Httpx client is not set. Please start the service first.")
        return self._httpx_client

    @property
    def endpoint(self) -> str:
        """
        Get the endpoint for the service.

        Returns:
            str: Endpoint URL.
        """
        return f"{self.service_uri}/v1"

    @property
    def api_key(self) -> str:
        """
        Get the API key for authentication.

        Returns:
            str: API key.
        """
        return os.getenv("OPENAI_API_KEY") or "OPENAI_API_KEY"

    # Service management api
    def is_service_running(self) -> bool:
        """
        Check if the service is running. Will also set the service URI if it is not set.

        Returns:
            bool: True if the service is running, False otherwise.
        """
        self._set_service_uri_and_client(get_service_uri())
        return self._service_uri is not None

    def start_service(self):
        """Start the service."""
        self._set_service_uri_and_client(start_service())

    # Catalog api
    def list_catalog_models(self) -> list[FoundryModelInfo]:
        """
        Get a list of available models in the catalog.

        Returns:
            list[FoundryModelInfo]: List of catalog models.
        """
        if self._catalog_list is None:
            self._catalog_list = [
                FoundryModelInfo.from_list_response(model) for model in self.httpx_client.get("/foundry/list")
            ]
        return self._catalog_list

    def _get_catalog_dict(self) -> dict[str, FoundryModelInfo]:
        """
        Get a dictionary of available models. Keyed by model ID and alias. Alias points to the most preferred model.

        Returns:
            dict[str, FoundryModelInfo]: Dictionary of catalog models.
        """
        if self._catalog_dict is not None:
            return self._catalog_dict

        catalog_models = self.list_catalog_models()
        self._catalog_dict = {model.id: model for model in catalog_models}
        alias_candidates = {}

        # Group models by alias
        for model in catalog_models:
            alias_candidates.setdefault(model.alias, []).append(model)

        # Define the preferred order of execution providers
        preferred_order = [
            ExecutionProvider.QNN,
            ExecutionProvider.CUDA,
            ExecutionProvider.CPU,
            ExecutionProvider.WEBGPU,
        ]
        if platform.system() != "Windows":
            # Adjust order for non-Windows platforms
            preferred_order.remove(ExecutionProvider.CPU)
            preferred_order.append(ExecutionProvider.CPU)

        priority_map = {provider: index for index, provider in enumerate(preferred_order)}

        # Choose the preferred model for each alias
        for alias, candidates in alias_candidates.items():
            self._catalog_dict[alias] = min(candidates, key=lambda model: priority_map.get(model.runtime, float("inf")))

        return self._catalog_dict

    def refresh_catalog(self):
        """Refresh the catalog."""
        self._catalog_list = None
        self._catalog_dict = None

    def get_model_info(self, alias_or_model_id: str, raise_on_not_found: bool = False) -> FoundryModelInfo | None:
        """
        Get the model information by alias or ID.

        Args:
            alias_or_model_id (str): Alias or Model ID. If it is an alias, the most preferred model will be returned.
            raise_on_not_found (bool): If True, raise an error if the model is not found. Default is False.

        Returns:
            FoundryModelInfo | None: Model information or None if not found.

        Raises:
            ValueError: If the model is not found and raise_on_not_found is True.
        """
        model_info = self._get_catalog_dict().get(alias_or_model_id)
        if model_info is None and raise_on_not_found:
            raise ValueError(f"Model {alias_or_model_id} not found in the catalog.")
        return model_info

    # Cache management api
    def get_cache_location(self):
        """
        Get the cache location.

        Returns:
            str: Path to the cache location.
        """
        return self.httpx_client.get("/openai/status")["modelDirPath"]

    def _fetch_model_infos(self, model_ids: list[str]) -> list[FoundryModelInfo]:
        """
        Fetch model information for a list of model IDs.

        Args:
            model_ids (list[str]): List of model IDs.

        Returns:
            list[FoundryModelInfo]: List of model information.
        """
        model_infos = []
        for model_id in model_ids:
            if (model_info := self.get_model_info(model_id)) is not None:
                model_infos.append(model_info)
            else:
                logger.debug("Model %s not found in the catalog.", model_id)
        return model_infos

    def list_cached_models(self) -> list[FoundryModelInfo]:
        """
        Get a list of cached models.

        Returns:
            list[FoundryModelInfo]: List of models downloaded to the cache.
        """
        return self._fetch_model_infos(self.httpx_client.get("/openai/models"))

    # Model management api
    def download_model(self, alias_or_model_id: str, token: str | None = None, force: bool = False) -> FoundryModelInfo:
        """
        Download a model.

        Args:
            alias_or_model_id (str): Alias or Model ID. If it is an alias, the most preferred model will be downloaded.
            token (str | None): Optional token for authentication.
            force (bool): If True, force download the model even if it is already downloaded.

        Returns:
            FoundryModelInfo: Model information.

        Raises:
            RuntimeError: If the model download fails.
        """
        model_info = self.get_model_info(alias_or_model_id, raise_on_not_found=True)
        if model_info in self.list_cached_models() and not force:
            logger.info(
                "Model with alias '%s' and ID '%s' is already downloaded. Use force=True to download it again.",
                model_info.alias,
                model_info.id,
            )
            return model_info
        logger.info("Downloading model with alias '%s' and ID '%s'...", model_info.alias, model_info.id)
        response_body = self.httpx_client.post_with_progress(
            "/openai/download",
            body={
                "model": model_info.to_download_body(),
                "token": token,
                "IgnorePipeReport": True,
            },
        )
        if not response_body.get("success", False):
            raise RuntimeError(
                f"Failed to download model with error: {response_body.get('errorMessage', 'Unknown error')}"
            )
        return model_info

    def is_model_upgradable(self, alias_or_model_id: str) -> bool:
        """
        Check if a newer version of a model is available.

        Args:
            alias_or_model_id (str): Alias or Model ID.

        Returns:
            bool: True if a newer version is available, False otherwise.

        Raises:
            ValueError: If the model is not found in the catalog.
        """
        model_info = self.get_model_info(alias_or_model_id, raise_on_not_found=True)
        logger.info("Checking if model '%s' (ID: '%s') is upgradable...", model_info.alias, model_info.id)

        response = self.httpx_client.get(f"/foundry/upgradable/{model_info.id}")
        return response.get("upgradable", False)

    def upgrade_model(self, alias_or_model_id: str, token: str | None = None) -> None:
        """
        Download the latest version of a model to the local cache.

        Args:
            alias_or_model_id (str): Alias or Model ID.
            token (str | None): Optional token for authentication.

        Raises:
            ValueError: If the model is not found in the catalog.
            RuntimeError: If the model upgrade fails.
        """
        model_info = self.get_model_info(alias_or_model_id, raise_on_not_found=True)
        logger.info("Upgrading model with alias '%s' and ID '%s'...", model_info.alias, model_info.id)

        upgrade_body = {
            "Name": model_info.id,
            "Uri": model_info.uri,
            "Publisher": model_info.publisher,
            "ProviderType": "AzureFoundryLocal" if model_info.provider == "AzureFoundry" else model_info.provider,
            "PromptTemplate": model_info.prompt_template,
        }
        body={
            "model": upgrade_body,
            "token": token,
            "IgnorePipeReport": True,
        },

        response_body = self.httpx_client.post_with_progress("/foundry/upgrade", body=body)

        if not response_body.get("success", False):
            raise RuntimeError(
                f"Failed to upgrade model with error: {response_body.get('errorMessage', 'Unknown error')}"
            )

        return model_info

    def load_model(self, alias_or_model_id: str, ttl: int = 600) -> FoundryModelInfo:
        """
        Load a model.

        Args:
            alias_or_model_id (str): Alias or Model ID. If it is an alias, the most preferred model will be loaded.
            ttl (int): Time to live for the model in seconds. Default is 600 seconds (10 minutes).

        Returns:
            FoundryModelInfo: Model information.

        Raises:
            ValueError: If the model is not in the catalog or has not been downloaded yet.
        """
        model_info = self.get_model_info(alias_or_model_id, raise_on_not_found=True)
        logger.info("Loading model with alias '%s' and ID '%s'...", model_info.alias, model_info.id)
        query_params = {"ttl": ttl}
        if model_info.runtime in {ExecutionProvider.WEBGPU, ExecutionProvider.CUDA}:
            # these models might have empty ep or dml ep in the genai config
            # use cuda if available, otherwise use the model's runtime
            has_cuda_support = any(mi.runtime == ExecutionProvider.CUDA for mi in self.list_catalog_models())
            query_params["ep"] = (
                ExecutionProvider.CUDA.get_alias() if has_cuda_support else model_info.runtime.get_alias()
            )
        try:
            self.httpx_client.get(f"/openai/load/{model_info.id}", query_params=query_params)
        except HttpResponseError as e:
            if "No OpenAIService provider found for modelName" in str(e):
                raise ValueError(
                    f"Model {alias_or_model_id} has not been downloaded yet. Please download it first."
                ) from None
            raise
        return model_info

    def unload_model(self, alias_or_model_id: str, force: bool = False):
        """
        Unload a model.

        Args:
            alias_or_model_id (str): Alias or Model ID.
            force (bool): If True, force unload a model with TTL.
        """
        model_info = self.get_model_info(alias_or_model_id, raise_on_not_found=True)
        if model_info not in self.list_loaded_models():
            # safest since unload fails if model is not downloaded, easier to check if loaded
            logger.info(
                "Model with alias '%s' and ID '%s' is not loaded. No need to unload.", model_info.alias, model_info.id
            )
            return
        logger.info("Unloading model with alias '%s' and ID '%s'...", model_info.alias, model_info.id)
        self.httpx_client.get(f"/openai/unload/{model_info.id}", query_params={"force": force})

    def list_loaded_models(self) -> list[FoundryModelInfo]:
        """
        Get a list of loaded models.

        Returns:
            list[FoundryModelInfo]: List of loaded models.
        """
        return self._fetch_model_infos(self.httpx_client.get("/openai/loadedmodels"))
