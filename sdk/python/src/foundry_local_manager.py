# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------

from __future__ import annotations

import json
import logging
import threading
import warnings

from typing import Optional

from .catalog import Catalog
from .configuration import Configuration
from .ep_types import EpDownloadResult, EpInfo
from .logging_helper import set_default_logger_severity
from .detail.core_interop import CoreInterop, InteropRequest
from .detail.model_load_manager import ModelLoadManager
from .exception import FoundryLocalException

logger = logging.getLogger(__name__)


class FoundryLocalManager:
    """Singleton manager for Foundry Local SDK operations.

    Call ``FoundryLocalManager.initialize(config)`` once at startup, then access
    the singleton via ``FoundryLocalManager.instance``.

    Attributes:
        instance: The singleton ``FoundryLocalManager`` instance (set after ``initialize``).
        catalog: The model ``Catalog`` for discovering and managing models.
        urls: Bound URL(s) after ``start_web_service()`` is called, or ``None``.
    """

    _lock = threading.Lock()
    instance: FoundryLocalManager = None

    @staticmethod
    def initialize(config: Configuration):
        """Initialize the Foundry Local SDK with the given configuration.

        This method must be called before using any other part of the SDK.

        Args:
            config: Configuration object for the SDK.
        """
        # Delegate singleton creation to the constructor, which enforces
        # the singleton invariant under a lock and sets `instance`.
        FoundryLocalManager(config)
        
    def __init__(self, config: Configuration):
        # Enforce singleton creation under a class-level lock and ensure
        # that `FoundryLocalManager.instance` is set exactly once.
        with FoundryLocalManager._lock:
            if FoundryLocalManager.instance is not None:
                raise FoundryLocalException(
                    "FoundryLocalManager is a singleton and has already been initialized."
                )
            config.validate()
            self.config = config
            self._initialize()
            FoundryLocalManager.instance = self

        self.urls = None

    def _initialize(self):
        set_default_logger_severity(self.config.log_level)

        external_service_url = self.config.web.external_url if self.config.web else None

        self._core_interop = CoreInterop(self.config)
        self._model_load_manager = ModelLoadManager(self._core_interop, external_service_url)
        self.catalog = Catalog(self._model_load_manager, self._core_interop)

    def discover_eps(self) -> list[EpInfo]:
        """Discover available execution providers and their registration status.

        Returns:
            List of ``EpInfo`` entries for all discoverable EPs.

        Raises:
            FoundryLocalException: If EP discovery fails or response JSON is invalid.
        """
        response = self._core_interop.execute_command("discover_eps")
        if response.error is not None:
            raise FoundryLocalException(f"Error discovering execution providers: {response.error}")

        try:
            payload = json.loads(response.data or "[]")
            return [EpInfo.from_dict(item) for item in payload]
        except Exception as e:
            raise FoundryLocalException(
                f"Failed to decode JSON response from discover_eps: {e}. Response was: {response.data}"
            ) from e

    def download_and_register_eps(self, names: Optional[list[str]] = None) -> EpDownloadResult:
        """Download and register execution providers (blocking).

        Args:
            names: Optional subset of EP names to download. If omitted or empty,
                all discoverable EPs are downloaded.

        Returns:
            ``EpDownloadResult`` describing operation status and per-EP outcomes.

        Raises:
            FoundryLocalException: If the operation fails or response JSON is invalid.
        """
        request = None
        if names is not None and len(names) > 0:
            request = InteropRequest(params={"Names": ",".join(names)})

        response = self._core_interop.execute_command("download_and_register_eps", request)
        if response.error is not None:
            raise FoundryLocalException(f"Error downloading execution providers: {response.error}")

        try:
            payload = json.loads(response.data or "{}")
            return EpDownloadResult.from_dict(payload)
        except Exception as e:
            raise FoundryLocalException(
                "Failed to decode JSON response from download_and_register_eps: "
                f"{e}. Response was: {response.data}"
            ) from e

    def start_web_service(self):
        """Start the optional web service.

        If provided, the service will be bound to the value of Configuration.web.urls.
        The default of http://127.0.0.1:0 will be used otherwise, which binds to a random ephemeral port.

        FoundryLocalManager.urls will be updated with the actual URL/s the service is listening on.
        """
        with FoundryLocalManager._lock:
            response = self._core_interop.execute_command("start_service")

            if response.error is not None:
                raise FoundryLocalException(f"Error starting web service: {response.error}")

            bound_urls = json.loads(response.data)
            if bound_urls is None or len(bound_urls) == 0:
                raise FoundryLocalException("Failed to get bound URLs from web service start response.")

            self.urls = bound_urls

    def stop_web_service(self):
        """Stop the optional web service."""

        with FoundryLocalManager._lock:
            if self.urls is None:
                raise FoundryLocalException("Web service is not running.")

            response = self._core_interop.execute_command("stop_service")

            if response.error is not None:
                raise FoundryLocalException(f"Error stopping web service: {response.error}")

            self.urls = None
