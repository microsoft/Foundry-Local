# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Python 3.10 compatibility shim for StrEnum.

Centralised here so that ``logging_helper`` and ``detail.utils`` (and any
future consumers) share a single definition without introducing circular
imports — this module has **no** intra-package dependencies.
"""

import sys

if sys.version_info >= (3, 11):
    from enum import StrEnum
else:
    from enum import Enum

    class StrEnum(str, Enum):
        def __str__(self) -> str:
            return self.value

__all__ = ["StrEnum"]
