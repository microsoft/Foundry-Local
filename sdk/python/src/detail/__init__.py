# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""This file is required for Python to treat this directory as a package,
enabling dotted imports such as ``foundry_local_sdk.detail.core_interop``.

The re-exports below are optional convenience aliases so callers can write
``from foundry_local_sdk.detail import CoreInterop`` instead of importing
from the individual submodule directly.
"""

from .core_interop import CoreInterop, InteropRequest, Response
from .model_data_types import ModelInfo, DeviceType, Runtime
from .model_load_manager import ModelLoadManager

__all__ = [
    "CoreInterop",
    "DeviceType",
    "InteropRequest",
    "ModelInfo",
    "ModelLoadManager",
    "Response",
    "Runtime",
]
