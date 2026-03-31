# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------

from typing import List

from pydantic import BaseModel, Field


class EpInfo(BaseModel):
    """Metadata describing a discoverable execution provider (EP)."""

    name: str = Field(alias="Name")
    is_registered: bool = Field(alias="IsRegistered")


class EpDownloadResult(BaseModel):
    """Result of an explicit EP download and registration operation."""

    success: bool = Field(alias="Success")
    status: str = Field(alias="Status")
    registered_eps: List[str] = Field(alias="RegisteredEps")
    failed_eps: List[str] = Field(alias="FailedEps")
