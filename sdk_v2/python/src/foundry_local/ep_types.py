# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
from __future__ import annotations

import dataclasses
from dataclasses import dataclass


@dataclass(frozen=True)
class EpInfo:
    """Metadata describing a discoverable execution provider (EP)."""

    name: str
    is_registered: bool

    def to_dict(self) -> dict:
        return dataclasses.asdict(self)

    @classmethod
    def from_dict(cls, d: dict) -> EpInfo:
        return cls(name=d["name"], is_registered=d["is_registered"])


@dataclass(frozen=True)
class EpDownloadResult:
    """Result of an explicit EP download and registration operation."""

    success: bool
    status: str
    registered_eps: list[str]
    failed_eps: list[str]

    def to_dict(self) -> dict:
        return dataclasses.asdict(self)

    @classmethod
    def from_dict(cls, d: dict) -> EpDownloadResult:
        return cls(
            success=d["success"],
            status=d["status"],
            registered_eps=d["registered_eps"],
            failed_eps=d["failed_eps"],
        )
