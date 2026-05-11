"""Private cffi interop layer for the Foundry Local SDK.

Public code should never import from this package directly — use
``foundry_local`` instead.
"""

from foundry_local._native.api import api, ffi

__all__ = ["ffi", "api"]
