# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Execution-provider metadata types.

These are plain ``@dataclass(frozen=True)`` types, not Pydantic models. This
is a deliberate departure from the legacy SDK, which used Pydantic for
``EpInfo`` and ``EpDownloadResult``. The SDK no longer depends on Pydantic
outside the optional ``openai`` compat layer; users who need JSON
serialization can call ``dataclasses.asdict()``.
"""
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
