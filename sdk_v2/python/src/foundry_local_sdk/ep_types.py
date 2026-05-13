# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class EpInfo:
    """Metadata describing a discoverable execution provider (EP)."""

    name: str
    is_registered: bool


@dataclass(frozen=True)
class EpDownloadResult:
    """Result of an explicit EP download and registration operation."""

    success: bool
    status: str
    registered_eps: list[str]
    failed_eps: list[str]
