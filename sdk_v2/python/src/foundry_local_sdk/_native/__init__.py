"""Private cffi interop layer for the Foundry Local SDK.

Public code should never import from this package directly — use
``foundry_local_sdk`` instead.
"""

from foundry_local_sdk._native.api import api, ffi

__all__ = ["ffi", "api"]
