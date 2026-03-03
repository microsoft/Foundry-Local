# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
from __future__ import annotations

import json
import logging
import threading

from src.catalog import Catalog
from src.configuration import Configuration
from src.logging_helper import set_default_logger_severity
from src.detail.core_interop import CoreInterop
from src.detail.model_load_manager import ModelLoadManager
from src.exception import FoundryLocalException

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
        FoundryLocalManager.instance = FoundryLocalManager(config)

    def __init__(self, config: Configuration):
        if not FoundryLocalManager.instance:
            with FoundryLocalManager._lock:
                if not FoundryLocalManager.instance:
                    config.validate()
                    self.config = config
                    self._initialize()
        else:
            raise Exception("FoundryLocalManager is a singleton and has already been initialized.")

        self.urls = None

    def _initialize(self):
        set_default_logger_severity(self.config.log_level)

        external_service_url = self.config.web.external_url if self.config.web else None

        self._core_interop = CoreInterop(self.config)
        self._model_load_manager = ModelLoadManager(self._core_interop, external_service_url)
        self.catalog = Catalog(self._model_load_manager, self._core_interop)

    def ensure_eps_downloaded(self) -> None:
        """Ensure execution providers are downloaded and registered (synchronous).
        Only relevant when using WinML.

        Raises:
            FoundryLocalException: If execution provider download fails.
        """
        result = self._core_interop.execute_command("ensure_eps_downloaded")

        if result.error is not None:
            raise FoundryLocalException(f"Error ensuring execution providers downloaded: {result.error}")

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
