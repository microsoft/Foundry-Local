# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
from __future__ import annotations

from dataclasses import dataclass
from enum import StrEnum

from typing_extensions import deprecated


class DeviceType(StrEnum):
    """Device types supported by model variants."""

    CPU = "CPU"
    GPU = "GPU"
    NPU = "NPU"


@deprecated(
    "PromptTemplate is an internal model implementation detail and will be removed in a future release. "
    "Templates are applied automatically by ChatSession."
)
@dataclass(frozen=True)
class PromptTemplate:
    """Prompt template strings for system, user, assistant, and raw prompt roles.

    .. deprecated::
        ``PromptTemplate`` is an internal model implementation detail and will be
        removed in a future release. Templates are applied automatically by
        ``ChatSession``; callers should not need to consume them directly.
    """

    system: str | None = None
    user: str | None = None
    assistant: str | None = None
    prompt: str | None = None


@dataclass(frozen=True)
class Runtime:
    """Runtime configuration specifying the device type and execution provider."""

    device_type: DeviceType
    execution_provider: str


@dataclass(frozen=True)
class Parameter:
    """A named parameter with an optional string value."""

    name: str
    value: str | None = None


@dataclass(frozen=True)
class ModelSettings:
    """Model-specific settings containing a list of parameters."""

    parameters: list[Parameter] | None = None


@dataclass(frozen=True)
class ModelInfo:
    """Catalog metadata for a single model variant."""

    id: str
    name: str
    version: int
    alias: str
    display_name: str | None
    provider_type: str
    uri: str
    model_type: str
    prompt_template: PromptTemplate | None
    """.. deprecated::
        ``prompt_template`` is an internal model implementation detail and will
        be removed in a future release. It is no longer populated from native
        catalog data; ``ChatSession`` applies templates automatically.
    """
    publisher: str | None
    model_settings: ModelSettings | None
    license: str | None
    license_description: str | None
    cached: bool
    task: str | None
    runtime: Runtime | None
    file_size_mb: int | None
    supports_tool_calling: bool | None
    max_output_tokens: int | None
    min_fl_version: str | None
    created_at_unix: int
    context_length: int | None
    input_modalities: str | None
    output_modalities: str | None
    capabilities: str | None

    def get_string_property(self, key: str) -> str | None:
        """Get a named property by key (for forward compatibility)."""
        return getattr(self, key.replace("-", "_"), None)

    def get_int_property(self, key: str, default: int = 0) -> int:
        """Get a named property as int (for forward compatibility)."""
        val = getattr(self, key.replace("-", "_"), None)
        return int(val) if val is not None else default
