# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
from __future__ import annotations

import dataclasses
from dataclasses import dataclass
from enum import StrEnum


class DeviceType(StrEnum):
    """Device types supported by model variants."""

    CPU = "CPU"
    GPU = "GPU"
    NPU = "NPU"


@dataclass(frozen=True)
class PromptTemplate:
    """Prompt template strings for system, user, assistant, and raw prompt roles."""

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

    def to_dict(self) -> dict:
        return dataclasses.asdict(self)

    @classmethod
    def from_dict(cls, d: dict) -> ModelInfo:
        pt_data = d.get("prompt_template")
        prompt_template = PromptTemplate(**pt_data) if pt_data else None

        rt_data = d.get("runtime")
        runtime = None
        if rt_data:
            runtime = Runtime(
                device_type=DeviceType(rt_data["device_type"]),
                execution_provider=rt_data["execution_provider"],
            )

        ms_data = d.get("model_settings")
        model_settings = None
        if ms_data:
            raw_params = ms_data.get("parameters") or []
            params = [Parameter(**p) for p in raw_params]
            model_settings = ModelSettings(parameters=params or None)

        return cls(
            id=d["id"],
            name=d["name"],
            version=d["version"],
            alias=d["alias"],
            display_name=d.get("display_name"),
            provider_type=d.get("provider_type", ""),
            uri=d.get("uri", ""),
            model_type=d.get("model_type", ""),
            prompt_template=prompt_template,
            publisher=d.get("publisher"),
            model_settings=model_settings,
            license=d.get("license"),
            license_description=d.get("license_description"),
            cached=d.get("cached", False),
            task=d.get("task"),
            runtime=runtime,
            file_size_mb=d.get("file_size_mb"),
            supports_tool_calling=d.get("supports_tool_calling"),
            max_output_tokens=d.get("max_output_tokens"),
            min_fl_version=d.get("min_fl_version"),
            created_at_unix=d.get("created_at_unix", 0),
            context_length=d.get("context_length"),
            input_modalities=d.get("input_modalities"),
            output_modalities=d.get("output_modalities"),
            capabilities=d.get("capabilities"),
        )

    def get_string_property(self, key: str) -> str | None:
        """Get a named property by key (for forward compatibility)."""
        return getattr(self, key.replace("-", "_"), None)

    def get_int_property(self, key: str, default: int = 0) -> int:
        """Get a named property as int (for forward compatibility)."""
        val = getattr(self, key.replace("-", "_"), None)
        return int(val) if val is not None else default
