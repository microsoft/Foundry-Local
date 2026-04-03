# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------

from typing import Optional, List
from pydantic import BaseModel, Field

from enum import StrEnum

# ---------- ENUMS ----------
class DeviceType(StrEnum):
    """Device types supported by model variants."""

    CPU = "CPU"
    GPU = "GPU"
    NPU = "NPU"

# ---------- DATA MODELS ----------

class PromptTemplate(BaseModel):
    """Prompt template strings for system, user, assistant, and raw prompt roles."""

    system: Optional[str] = Field(default=None, alias="system")
    user: Optional[str] = Field(default=None, alias="user")
    assistant: Optional[str] = Field(default=None, alias="assistant")
    prompt: Optional[str] = Field(default=None, alias="prompt")


class Runtime(BaseModel):
    """Runtime configuration specifying the device type and execution provider."""

    device_type: DeviceType = Field(alias="deviceType")
    execution_provider: str = Field(alias="executionProvider")


class Parameter(BaseModel):
    """A named parameter with an optional string value."""

    name: str
    value: Optional[str] = None


class ModelSettings(BaseModel):
    """Model-specific settings containing a list of parameters."""

    parameters: Optional[List[Parameter]] = Field(default=None, alias="parameters")


class ModelInfo(BaseModel):
    """Catalog metadata for a single model variant.

    Fields are populated from the JSON response of the ``get_model_list`` command.
    """

    id: str = Field(alias="id", description="Unique identifier of the model. Generally <name>:<version>")
    name: str = Field(alias="name", description="Model variant name")
    version: int = Field(alias="version")
    alias: str = Field(..., description="Alias of the model")
    display_name: Optional[str] = Field(default=None, alias="displayName")
    provider_type: str = Field(alias="providerType")
    uri: str = Field(alias="uri")
    model_type: str = Field(alias="modelType")
    prompt_template: Optional[PromptTemplate] = Field(default=None, alias="promptTemplate")
    publisher: Optional[str] = Field(default=None, alias="publisher")
    model_settings: Optional[ModelSettings] = Field(default=None, alias="modelSettings")
    license: Optional[str] = Field(default=None, alias="license")
    license_description: Optional[str] = Field(default=None, alias="licenseDescription")
    cached: bool = Field(alias="cached")
    task: Optional[str] = Field(default=None, alias="task")
    runtime: Optional[Runtime] = Field(default=None, alias="runtime")
    file_size_mb: Optional[int] = Field(default=None, alias="fileSizeMb")
    supports_tool_calling: Optional[bool] = Field(default=None, alias="supportsToolCalling")
    max_output_tokens: Optional[int] = Field(default=None, alias="maxOutputTokens")
    min_fl_version: Optional[str] = Field(default=None, alias="minFLVersion")
    created_at_unix: int = Field(alias="createdAt")
    context_length: Optional[int] = Field(default=None, alias="contextLength")
    input_modalities: Optional[str] = Field(default=None, alias="inputModalities")
    output_modalities: Optional[str] = Field(default=None, alias="outputModalities")
    capabilities: Optional[str] = Field(default=None, alias="capabilities")
