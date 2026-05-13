"""_Api singleton: loads the flApi vtable and provides status-to-exception conversion.

Usage (internal only — public code never imports from _native)::

    from foundry_local_sdk._native.api import api

    api.check_status(api.root.Manager_Create(cfg_ptr, out_mgr))
    version = api.version_string()
"""

from __future__ import annotations

import importlib
import os

from foundry_local_sdk._native.lib_loader import find_library, prepare_native_dependencies
from foundry_local_sdk.exception import FoundryLocalException

# FOUNDRY_LOCAL_API_VERSION = 1 (from foundry_local_c.h)
_FOUNDRY_LOCAL_API_VERSION: int = 1

_lib_path = find_library()

# Wire up ORT/GenAI native deps BEFORE we touch the DLL search path or import the cffi extension. On Windows
# this calls os.add_dll_directory for each PyPI-shipped ORT/GenAI package dir; on Linux/macOS it materialises
# the lib* symlink workaround next to foundry_local. Best-effort and never raises.
if _lib_path is not None:
    prepare_native_dependencies(_lib_path.parent)

# On Windows, ensure the DLL directory is in the search path before importing the cffi extension. The extension
# was linked against foundry_local.dll at build time, so Windows needs to locate it at import time. When
# ``_lib_path`` is None the system search path will be used as-is (no CWD pollution via a relative path).
if _lib_path is not None and hasattr(os, "add_dll_directory"):
    _dll_parent = _lib_path.parent.resolve()
    if _dll_parent.is_dir():
        os.add_dll_directory(str(_dll_parent))

# Import the compiled cffi extension (API mode: struct layouts verified at
# build time, function calls compiled-in rather than interpreted).
_cffi_mod = importlib.import_module("foundry_local_sdk._native._cffi_bindings")
ffi = _cffi_mod.ffi
_lib = _cffi_mod.lib


class _Api:
    """Thin wrapper around the flApi vtable returned by FoundryLocalGetApi.

    Acquires all sub-API pointers at construction time so callers never have
    to call the accessor functions themselves.

    This object is a process-level singleton (``api`` below).  Do not create
    additional instances.
    """

    def __init__(self) -> None:
        root_ptr = _lib.FoundryLocalGetApi(_FOUNDRY_LOCAL_API_VERSION)
        if root_ptr == ffi.NULL:
            raise FoundryLocalException(
                f"FoundryLocalGetApi({_FOUNDRY_LOCAL_API_VERSION}) returned NULL — "
                "library version mismatch or corrupt binary.",
                error_code=0,
            )
        # root is const flApi* — access function pointer members directly.
        self.root = root_ptr
        self.item = self.root.GetItemApi()
        self.inference = self.root.GetInferenceApi()
        self.config = self.root.GetConfigurationApi()
        self.catalog = self.root.GetCatalogApi()
        self.model = self.root.GetModelApi()

    def check_status(self, status: object) -> None:
        """Raise ``FoundryLocalException`` if *status* is non-NULL.

        Always releases *status* so callers never need to call
        ``Status_Release`` explicitly.
        """
        if status == ffi.NULL:
            return
        try:
            code = self.root.Status_GetErrorCode(status)
            msg_ptr = self.root.Status_GetErrorMessage(status)
            msg = ffi.string(msg_ptr).decode("utf-8") if msg_ptr != ffi.NULL else "Unknown error"
        finally:
            self.root.Status_Release(status)
        raise FoundryLocalException(msg, error_code=int(code))

    @staticmethod
    def version_string() -> str:
        """Return the library version string from ``FoundryLocalGetVersionString``."""
        ptr = _lib.FoundryLocalGetVersionString()
        if ptr == ffi.NULL:
            return ""
        return ffi.string(ptr).decode("utf-8")


class _LazyApi:
    """Lazy proxy that constructs the ``_Api`` singleton on first attribute access.

    Deferring construction keeps module import cheap and — more importantly — makes a failed
    ``FoundryLocalGetApi`` call recoverable. If construction were eager at import time, Python would cache the
    ``ImportError`` permanently and no later ``import`` could retry, even after the underlying problem (missing
    DLL on PATH, bad version) was fixed in the same process.

    Forwards every attribute (including the ``version_string`` ``@staticmethod``) through normal attribute
    lookup on the underlying singleton.
    """

    __slots__ = ("_instance",)

    def __init__(self) -> None:
        self._instance: _Api | None = None

    def _get(self) -> _Api:
        inst = self._instance
        if inst is None:
            inst = _Api()
            self._instance = inst
        return inst

    def __getattr__(self, name: str) -> object:
        return getattr(self._get(), name)


# Process-level singleton (lazily constructed on first attribute access).
api = _LazyApi()
