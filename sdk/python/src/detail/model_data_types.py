# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------

from typing import Optional, List
from pydantic import BaseModel, Field

from .utils import StrEnum

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
    display_name: Optional[str] = Field(alias="displayName")
    provider_type: str = Field(alias="providerType")
    uri: str = Field(alias="uri")
    model_type: str = Field(alias="modelType")
    prompt_template: Optional[PromptTemplate] = Field(default=None, alias="promptTemplate")
    publisher: Optional[str] = Field(alias="publisher")
    model_settings: Optional[ModelSettings] = Field(default=None, alias="modelSettings")
    license: Optional[str] = Field(alias="license")
    license_description: Optional[str] = Field(alias="licenseDescription")
    cached: bool = Field(alias="cached")
    task: Optional[str] = Field(alias="task")
    runtime: Optional[Runtime] = Field(alias="runtime")
    file_size_mb: Optional[int] = Field(alias="fileSizeMb")
    supports_tool_calling: Optional[bool] = Field(alias="supportsToolCalling")
    max_output_tokens: Optional[int] = Field(alias="maxOutputTokens")
    min_fl_version: Optional[str] = Field(alias="minFLVersion")
    created_at_unix: int = Field(alias="createdAt")
