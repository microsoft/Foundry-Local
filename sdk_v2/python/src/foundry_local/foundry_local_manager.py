# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
from __future__ import annotations

import threading
from typing import Callable

from foundry_local.catalog import Catalog
from foundry_local.configuration import Configuration
from foundry_local.ep_types import EpDownloadResult, EpInfo
from foundry_local.exception import FoundryLocalException
from foundry_local.logging_helper import set_default_logger_severity


class FoundryLocalManager:
    """Singleton manager for Foundry Local SDK operations.

    Call ``FoundryLocalManager.initialize(config)`` once at startup, then
    access the singleton via ``FoundryLocalManager.instance``.

    Attributes:
        instance: The singleton ``FoundryLocalManager`` instance.
        catalog: The model ``Catalog`` for discovering and managing models.
        urls: Bound URL(s) after ``start_web_service()`` is called, or ``None``.
    """

    _lock: threading.Lock = threading.Lock()
    instance: FoundryLocalManager | None = None

    @staticmethod
    def initialize(config: Configuration) -> None:
        """Initialize the Foundry Local SDK with the given configuration.

        Must be called before using any other part of the SDK.

        Args:
            config: Configuration object for the SDK.
        """
        FoundryLocalManager(config)

    def __init__(self, config: Configuration) -> None:
        # Declared up front so close() / __del__ can run safely even if
        # _initialize() raises before the native handle is assigned.
        self._native_manager: object | None = None
        self.urls: list[str] | None = None

        with FoundryLocalManager._lock:
            if FoundryLocalManager.instance is not None:
                raise FoundryLocalException(
                    "FoundryLocalManager is a singleton and has already been initialized."
                )
            config.validate()
            self.config = config
            self._initialize()
            FoundryLocalManager.instance = self

    def _initialize(self) -> None:
        from foundry_local._native.api import api, ffi

        set_default_logger_severity(self.config.log_level)

        native_config = self.config._build_native()
        try:
            mgr_out = ffi.new("flManager**")
            api.check_status(api.root.Manager_Create(native_config, mgr_out))
            self._native_manager = mgr_out[0]
        finally:
            # Manager_Create takes a const flConfiguration* — it copies what it needs.
            # We own the config handle and release it now.
            api.config.Configuration_Release(native_config)

        cat_out = ffi.new("flCatalog**")
        api.check_status(api.root.Manager_GetCatalog(self._native_manager, cat_out))
        self.catalog = Catalog(cat_out[0])

    # ------------------------------------------------------------------
    # EP discovery and registration
    # ------------------------------------------------------------------

    def discover_eps(self) -> list[EpInfo]:
        """Discover available execution providers and their registration status.

        Returns:
            List of ``EpInfo`` entries for all discoverable EPs.
        """
        from foundry_local._native.api import api, ffi

        names_out = ffi.new("char***")
        is_reg_out = ffi.new("int**")
        count_out = ffi.new("size_t*")
        api.check_status(
            api.root.Manager_GetDiscoverableEps(
                self._native_manager, names_out, is_reg_out, count_out
            )
        )

        count = int(count_out[0])
        result: list[EpInfo] = []
        for i in range(count):
            name = ffi.string(names_out[0][i]).decode("utf-8")
            is_reg = bool(is_reg_out[0][i])
            result.append(EpInfo(name=name, is_registered=is_reg))
        return result

    def download_and_register_eps(
        self,
        names: list[str] | None = None,
        progress_callback: Callable[[str, float], None] | None = None,
    ) -> EpDownloadResult:
        """Download and register execution providers.

        Args:
            names: Optional subset of EP names to download.  If omitted or
                empty, all discoverable EPs are downloaded.
            progress_callback: Optional callback ``(ep_name: str, percent: float)``
                invoked as each EP downloads.  ``percent`` is 0–100.

        Returns:
            ``EpDownloadResult`` describing operation status and per-EP outcomes.
        """
        from foundry_local._native.api import api, ffi

        # Snapshot before-state to compute the delta of newly registered EPs.
        before_eps: dict[str, bool] = {ep.name: ep.is_registered for ep in self.discover_eps()}

        # Build the native EP-names array (or NULL to download all).
        if names:
            # Keep encoded byte strings alive for the duration of the call.
            c_name_bufs = [ffi.new("char[]", n.encode("utf-8")) for n in names]
            c_names_arr = ffi.new("const char*[]", c_name_bufs)
            num_names = len(names)
        else:
            c_names_arr = ffi.NULL
            num_names = 0

        # Build the progress callback trampoline if requested.
        cb = ffi.NULL
        ud = ffi.NULL
        if progress_callback is not None:
            self._ep_cb_handle = ffi.new_handle(progress_callback)

            @ffi.callback("int(const char*, float, void*)")
            def _ep_cb(ep_name_ptr: object, value: float, user_data: object) -> int:
                try:
                    fn = ffi.from_handle(user_data)
                    ep_name = (
                        ffi.string(ep_name_ptr).decode("utf-8")
                        if ep_name_ptr != ffi.NULL
                        else ""
                    )
                    fn(ep_name, float(value))
                    return 0
                except Exception:
                    return 1

            self._ep_cb = _ep_cb  # prevent GC
            cb = _ep_cb
            ud = self._ep_cb_handle

        api.check_status(
            api.root.Manager_DownloadAndRegisterEps(
                self._native_manager, c_names_arr, num_names, cb, ud
            )
        )

        # Determine which EPs were newly registered.
        after_eps = self.discover_eps()
        registered = [
            ep.name
            for ep in after_eps
            if ep.is_registered and not before_eps.get(ep.name, False)
        ]
        failed = [
            ep.name
            for ep in after_eps
            if not ep.is_registered and ep.name in (names or [])
        ]

        result = EpDownloadResult(
            success=len(failed) == 0,
            status="Completed",
            registered_eps=registered,
            failed_eps=failed,
        )

        # Invalidate the catalog cache so the next access re-fetches with updated EPs.
        if result.success or registered:
            self.catalog._invalidate_cache()

        return result

    # ------------------------------------------------------------------
    # Web service lifecycle
    # ------------------------------------------------------------------

    def start_web_service(self) -> None:
        """Start the optional built-in web service.

        The service binds to the URL(s) specified in ``Configuration.web.urls``,
        or ``http://127.0.0.1:0`` (a random ephemeral port) if not specified.
        ``FoundryLocalManager.urls`` is updated with the actual bound URL(s).
        """
        from foundry_local._native.api import api, ffi

        api.check_status(api.root.Manager_WebServiceStart(self._native_manager))

        urls_out = ffi.new("char***")
        count_out = ffi.new("size_t*")
        api.check_status(
            api.root.Manager_WebServiceUrls(self._native_manager, urls_out, count_out)
        )
        self.urls = [
            ffi.string(urls_out[0][i]).decode("utf-8") for i in range(int(count_out[0]))
        ]

    def stop_web_service(self) -> None:
        """Stop the optional built-in web service.

        Raises:
            FoundryLocalException: If the web service is not currently running.
        """
        from foundry_local._native.api import api

        if self.urls is None:
            raise FoundryLocalException("Web service is not running.")

        api.check_status(api.root.Manager_WebServiceStop(self._native_manager))
        self.urls = None

    # ------------------------------------------------------------------
    # Shutdown
    # ------------------------------------------------------------------

    def shutdown(self) -> None:
        """Initiate graceful shutdown of the native manager.

        Safe to call from any thread. Idempotent.
        """
        from foundry_local._native.api import api

        api.check_status(api.root.Manager_Shutdown(self._native_manager))

    def is_shutdown_requested(self) -> bool:
        """Whether ``shutdown()`` has been called on the native manager."""
        from foundry_local._native.api import api

        return bool(api.root.Manager_IsShutdownRequested(self._native_manager))

    def close(self) -> None:
        """Tear down the native manager and clear the singleton.

        After ``close()`` returns, ``FoundryLocalManager.instance`` is ``None`` and
        a fresh ``FoundryLocalManager(config)`` may be constructed. The native
        side enforces single-instance semantics via its own singleton; this method
        drives the native ``Manager_Shutdown`` (which drains sessions and unloads
        models) before releasing the handle.

        Idempotent. Safe to call multiple times.
        """
        from foundry_local._native.api import api

        with FoundryLocalManager._lock:
            # Idempotent — close() called twice or after a failed __init__.
            if self._native_manager is None:
                if FoundryLocalManager.instance is self:
                    FoundryLocalManager.instance = None
                return

            # Drive the orchestrated drain on the native side.
            try:
                api.check_status(api.root.Manager_Shutdown(self._native_manager))
            except Exception:
                # Best-effort; we still need to release.
                pass

            try:
                api.root.Manager_Release(self._native_manager)
            finally:
                self._native_manager = None
                if FoundryLocalManager.instance is self:
                    FoundryLocalManager.instance = None

    def __enter__(self) -> "FoundryLocalManager":
        return self

    def __exit__(self, *_: object) -> None:
        self.close()

    def __del__(self) -> None:
        # Best-effort safety net — production code should call close() explicitly.
        try:
            self.close()
        except Exception:
            pass
