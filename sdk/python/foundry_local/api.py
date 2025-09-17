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
from foundry_local.models import DeviceType, ExecutionProvider, FoundryModelInfo
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

    """
    Extract numeric version from ID (e.g. model-x:3 → 3)

    Returns:
        int: Numeric version extracted from the model ID, or -1 if not found.
    """

    def _get_version(self, model_id: str) -> int:
        try:
            return int(model_id.split(":")[-1])
        except (ValueError, IndexError):
            return -1

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
            ExecutionProvider.CUDA,
            ExecutionProvider.CPU,
            ExecutionProvider.WEBGPU,
        ]
        if platform.system() != "Windows":
            # Adjust order for non-Windows platforms
            preferred_order.remove(ExecutionProvider.CPU)
            preferred_order.append(ExecutionProvider.CPU)

        # start from 1 since we want NPU to have the highest priority (0)
        priority_map = {provider: index + 1 for index, provider in enumerate(preferred_order)}

        def _get_priority(m: FoundryModelInfo) -> int:
            return 0 if m.device_type == DeviceType.NPU else priority_map.get(m.execution_provider, float("inf"))

        # Choose the best model for each alias based on priority and version
        for alias, candidates in alias_candidates.items():
            best_candidate = max(
                candidates,
                key=lambda m: (
                    -_get_priority(m),  # negate to mimic ascending priority
                    self._get_version(m.id),  # pick the highest version
                ),
            )
            self._catalog_dict[alias] = best_candidate

        return self._catalog_dict

    def refresh_catalog(self):
        """Refresh the catalog."""
        self._catalog_list = None
        self._catalog_dict = None

    def get_model_info(self, alias_or_model_id: str, raise_on_not_found: bool = False) -> FoundryModelInfo | None:
        """
        Get the model information of the latest model that matches the given alias or ID.

        Args:
            alias_or_model_id (str): Alias or Model ID. If it is an alias, the most preferred model will be returned.
                                     If it is a model ID, it can contain a ":<version>" suffix or not.
            raise_on_not_found (bool): If True, raise an error if the model is not found. Default is False.

        Returns:
            FoundryModelInfo | None: Model information or None if not found.

        Raises:
            ValueError: If the model is not found and raise_on_not_found is True.
        """
        catalog = self._get_catalog_dict()
        model_info = None

        # id with version, or alias
        if alias_or_model_id in catalog:
            model_info = catalog[alias_or_model_id]
        elif ":" not in alias_or_model_id:
            # alias_or_model_id is an id that does not contain a version
            prefix = f"{alias_or_model_id}:"
            best_version = -1

            for key, info in catalog.items():
                if key.startswith(prefix):
                    try:
                        version = self._get_version(key)
                        if version > best_version:
                            best_version = version
                            model_info = info
                    except ValueError:
                        continue  # Skip if version is not numeric

        if model_info is None and raise_on_not_found:
            raise ValueError(f"Model {alias_or_model_id} not found in the catalog.")
        return model_info

    def _get_latest_model_info(
        self, alias_or_model_id: str, raise_on_not_found: bool = False
    ) -> FoundryModelInfo | None:
        """
        Get the latest model information by alias or model ID.
        The difference from get_model_info is that this method will return the latest version of the model
        even when you pass it a model id that contains a version suffix.

        Args:
            alias_or_model_id (str): Alias or Model ID. If it is an alias, the most preferred model will be returned.
            raise_on_not_found (bool): If True, raise an error if the model is not found. Default is False.

        Returns:
            FoundryModelInfo | None: Latest model information or None if not found.

        Raises:
            ValueError: If the model is not found and raise_on_not_found is True.
        """
        if not alias_or_model_id:
            if raise_on_not_found:
                raise ValueError("The provided nodel alias or ID was empty.")
            return None

        # remove the ":<version>" suffix if it exists, and use it to get the latest model
        alias_or_name_without_version = alias_or_model_id.split(":")[0]
        return self.get_model_info(alias_or_name_without_version, raise_on_not_found)

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

    def is_model_upgradeable(self, alias_or_model_id: str) -> bool:
        """
        Check if a newer version of a model is available.

        Args:
            alias_or_model_id (str): Alias or Model ID.

        Returns:
            bool: True if a newer version is available, False otherwise.

        Raises:
            ValueError: If the model is not found in the catalog.
        """
        logger.info("Checking if model '%s' is upgradeable...", alias_or_model_id)
        model_info = self._get_latest_model_info(alias_or_model_id, raise_on_not_found=True)
        if model_info is None:
            return False  # Model not found in the catalog

        latest_version = self._get_version(model_info.id)
        if latest_version == -1:
            return False  # Invalid model ID format

        cached_models = self.list_cached_models()
        for cached_model in cached_models:
            if cached_model.id == model_info.id and self._get_version(cached_model.id) == latest_version:
                return False  # Cached model is already at the latest version

        return True  # The latest version is not in the cache

    def upgrade_model(self, alias_or_model_id: str, token: str | None = None) -> None:
        """
        Download the latest version of a model to the local cache, if the latest version is not already cached.

        Args:
            alias_or_model_id (str): Alias or Model ID.
            token (str | None): Optional token for authentication.

        Raises:
            ValueError: If the model is not found in the catalog.
            RuntimeError: If the model upgrade fails.
        """
        model_info = self._get_latest_model_info(alias_or_model_id, raise_on_not_found=True)
        return self.download_model(model_info.id, token=token)

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
        if model_info.execution_provider in {ExecutionProvider.WEBGPU, ExecutionProvider.CUDA}:
            # these models might have empty ep or dml ep in the genai config
            # use cuda if available, otherwise use the model's runtime
            has_cuda_support = any(mi.execution_provider == ExecutionProvider.CUDA for mi in self.list_catalog_models())
            query_params["ep"] = ExecutionProvider.CUDA.get_alias() if has_cuda_support else None
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
