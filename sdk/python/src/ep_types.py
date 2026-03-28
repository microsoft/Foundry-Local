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

    @staticmethod
    def from_dict(data: dict) -> "EpInfo":
        """Create ``EpInfo`` from core JSON payload (PascalCase fields)."""
        return EpInfo(
            name=data["Name"],
            is_registered=data["IsRegistered"],
        )


@dataclass(frozen=True)
class EpDownloadResult:
    """Result of an explicit EP download and registration operation."""

    success: bool
    status: str
    registered_eps: list[str]
    failed_eps: list[str]

    @staticmethod
    def from_dict(data: dict) -> "EpDownloadResult":
        """Create ``EpDownloadResult`` from core JSON payload (PascalCase fields)."""
        return EpDownloadResult(
            success=data["Success"],
            status=data["Status"],
            registered_eps=data["RegisteredEps"],
            failed_eps=data["FailedEps"],
        )
