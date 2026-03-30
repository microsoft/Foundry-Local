# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------

from __future__ import annotations

import json
import logging
import threading

from .catalog import Catalog
from .configuration import Configuration
from .logging_helper import set_default_logger_severity
from .detail.core_interop import CoreInterop
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

    def discover_eps(self) -> list[dict]:
        """Discover the execution providers available for download and registration.

        Returns:
            A list of dicts with 'Name' (str) and 'IsRegistered' (bool) keys.

        Raises:
            FoundryLocalException: If the discovery command fails.
        """
        result = self._core_interop.execute_command("discover_eps")
        if result.error is not None:
            raise FoundryLocalException(f"Error discovering execution providers: {result.error}")
        return json.loads(result.data) if result.data else []

    def ensure_eps_downloaded(self, names=None, progress_callback=None) -> None:
        """Ensure execution providers are downloaded and registered.
        Only relevant when using WinML.

        Args:
            names: Optional list of EP names to download. If None, all discoverable EPs are downloaded.
            progress_callback: Optional callback receiving per-EP progress updates.
                Called with (ep_name: str, percent: float) for each progress update.

        Raises:
            FoundryLocalException: If execution provider download fails.
        """
        input_params = None
        if names:
            from .detail.core_interop import InteropRequest
            input_params = InteropRequest(params={"Names": ",".join(names)})

        if progress_callback is not None:
            def _on_chunk(chunk: str):
                sep_index = chunk.find('|')
                if sep_index >= 0:
                    name = chunk[:sep_index] or None
                    try:
                        percent = float(chunk[sep_index + 1:])
                    except ValueError:
                        percent = 0.0
                    progress_callback(name, percent)

            result = self._core_interop.execute_command_with_callback(
                "download_and_register_eps", input_params, _on_chunk)
        else:
            result = self._core_interop.execute_command("download_and_register_eps", input_params)

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
