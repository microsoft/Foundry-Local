# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------

from __future__ import annotations

import asyncio
import json
import logging
import threading

from typing import Callable, List, Optional

from pydantic import TypeAdapter

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

    Call ``await FoundryLocalManager.initialize(config)`` once at startup, then
    access the singleton via ``FoundryLocalManager.instance``.

    Attributes:
        instance: The singleton ``FoundryLocalManager`` instance (set after ``initialize``).
        catalog: The model ``Catalog`` for discovering and managing models.
        urls: Bound URL(s) after ``start_web_service()`` is called, or ``None``.
    """

    _lock = threading.Lock()
    instance: FoundryLocalManager = None

    @staticmethod
    async def initialize(config: Configuration):
        """Initialize the Foundry Local SDK with the given configuration.

        This coroutine must be awaited before using any other part of the SDK::

            await FoundryLocalManager.initialize(config)
            manager = FoundryLocalManager.instance

        Args:
            config: Configuration object for the SDK.
        """
        # Run the synchronous constructor in a thread to avoid blocking
        # the event loop during native library initialization.
        await asyncio.to_thread(FoundryLocalManager, config)
        
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
        self._async_lock = asyncio.Lock()

    def _initialize(self):
        set_default_logger_severity(self.config.log_level)

        external_service_url = self.config.web.external_url if self.config.web else None

        self._core_interop = CoreInterop(self.config)
        self._model_load_manager = ModelLoadManager(self._core_interop, external_service_url)
        self.catalog = Catalog(self._model_load_manager, self._core_interop)

    async def discover_eps(self) -> list[EpInfo]:
        """Discover available execution providers and their registration status.

        Returns:
            List of ``EpInfo`` entries for all discoverable EPs.

        Raises:
            FoundryLocalException: If EP discovery fails or response JSON is invalid.
        """
        response = await self._core_interop.execute_command("discover_eps")
        if response.error is not None:
            raise FoundryLocalException(f"Error discovering execution providers: {response.error}")

        try:
            adapter = TypeAdapter(List[EpInfo])
            return adapter.validate_json(response.data or "[]")
        except Exception as e:
            raise FoundryLocalException(
                f"Failed to decode JSON response from discover_eps: {e}. Response was: {response.data}"
            ) from e

    async def download_and_register_eps(
        self,
        names: Optional[list[str]] = None,
        progress_callback: Optional[Callable[[str, float], None]] = None,
    ) -> EpDownloadResult:
        """Download and register execution providers.

        Args:
            names: Optional subset of EP names to download. If omitted or empty,
                all discoverable EPs are downloaded.
            progress_callback: Optional callback ``(ep_name: str, percent: float) -> None``
                invoked as each EP downloads. ``percent`` is 0-100.

        Returns:
            ``EpDownloadResult`` describing operation status and per-EP outcomes.

        Raises:
            FoundryLocalException: If the operation fails or response JSON is invalid.
        """
        request = None
        if names is not None and len(names) > 0:
            request = InteropRequest(params={"Names": ",".join(names)})

        if progress_callback is not None:
            loop = asyncio.get_running_loop()

            def _on_chunk(chunk: str) -> None:
                sep = chunk.find("|")
                if sep >= 0:
                    ep_name = chunk[:sep] or ""
                    try:
                        percent = float(chunk[sep + 1:])
                        loop.call_soon_threadsafe(progress_callback, ep_name, percent)
                    except ValueError:
                        pass

            response = await self._core_interop.execute_command_with_callback(
                "download_and_register_eps", request, _on_chunk
            )
        else:
            response = await self._core_interop.execute_command("download_and_register_eps", request)

        if response.error is not None:
            raise FoundryLocalException(f"Error downloading execution providers: {response.error}")

        if response.data:
            try:
                adapter = TypeAdapter(EpDownloadResult)
                ep_result = adapter.validate_json(response.data)
            except Exception as e:
                raise FoundryLocalException(
                    "Failed to decode JSON response from download_and_register_eps: "
                    f"{e}. Response was: {response.data}"
                ) from e
        else:
            ep_result = EpDownloadResult(
                Success=True, Status="Completed", RegisteredEps=[], FailedEps=[]
            )

        # Invalidate the catalog cache if any EP was newly registered so the next access
        # re-fetches models with the updated set of available EPs.
        if ep_result.success or len(ep_result.registered_eps) > 0:
            await self.catalog._invalidate_cache()

        return ep_result

    async def start_web_service(self):
        """Start the optional web service.

        If provided, the service will be bound to the value of Configuration.web.urls.
        The default of http://127.0.0.1:0 will be used otherwise, which binds to a random ephemeral port.

        FoundryLocalManager.urls will be updated with the actual URL/s the service is listening on.
        """
        async with self._async_lock:
            response = await self._core_interop.execute_command("start_service")

            if response.error is not None:
                raise FoundryLocalException(f"Error starting web service: {response.error}")

            bound_urls = json.loads(response.data)
            if bound_urls is None or len(bound_urls) == 0:
                raise FoundryLocalException("Failed to get bound URLs from web service start response.")

            self.urls = bound_urls

    async def stop_web_service(self):
        """Stop the optional web service."""

        async with self._async_lock:
            if self.urls is None:
                raise FoundryLocalException("Web service is not running.")

            response = await self._core_interop.execute_command("stop_service")

            if response.error is not None:
                raise FoundryLocalException(f"Error stopping web service: {response.error}")

            self.urls = None
