# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
from __future__ import annotations

import sys

from pydantic import BaseModel, Field

if sys.version_info >= (3, 11):
    from enum import StrEnum

else:
    from enum import Enum

    class StrEnum(str, Enum):
        def __str__(self) -> str:
            return self.value


# ruff: noqa: N815


class DeviceType(StrEnum):
    """Enumeration of devices supported by the model."""

    CPU = "CPU"
    GPU = "GPU"
    NPU = "NPU"


class ExecutionProvider(StrEnum):
    """Enumeration of common execution providers supported by the model."""

    CPU = "CPUExecutionProvider"
    WEBGPU = "WebGpuExecutionProvider"
    CUDA = "CUDAExecutionProvider"

    def get_alias(self) -> str:
        """
        Get the alias for the execution provider.

        Returns:
            str: Alias of the execution provider.
        """
        return self.value.replace("ExecutionProvider", "").lower()


class ModelRuntime(BaseModel):
    """Model runtime information."""

    deviceType: DeviceType = Field(..., description="Device type supported by the model")
    # use a string since we don't want to hardcode all possible execution providers
    executionProvider: str = Field(
        ...,
        description="Execution provider supported by the model",
    )


class FoundryListResponseModel(BaseModel):
    """Response model for listing models."""

    name: str = Field(..., description="Name of the model")
    displayName: str = Field(..., description="Display name of the model")
    modelType: str = Field(..., description="Type of the model")
    providerType: str = Field(..., description="Provider type of the model")
    uri: str = Field(..., description="URI of the model")
    version: str = Field(..., description="Version of the model")
    promptTemplate: dict | None = Field(..., description="Prompt template for the model")
    publisher: str = Field(..., description="Publisher of the model")
    task: str = Field(..., description="Task of the model")
    runtime: ModelRuntime = Field(..., description="Runtime information of the model")
    fileSizeMb: int = Field(..., description="File size of the model in MB")
    modelSettings: dict = Field(..., description="Model settings")
    alias: str = Field(..., description="Alias name of the model")
    supportsToolCalling: bool = Field(..., description="Whether the model supports tool calling")
    license: str = Field(..., description="License of the model")
    licenseDescription: str = Field(..., description="License description of the model")
    parentModelUri: str = Field(..., description="Parent model URI of the model")
    maxOutputTokens: int | None = Field(..., description="Maximum output tokens for the model")
    minFLVersion: str | None = Field(None, description="Minimum Foundry Local version required for the model")


class FoundryModelInfo(BaseModel):
    """Model information."""

    alias: str = Field(..., description="Alias of the model")
    id: str = Field(..., description="Unique identifier of the model")
    version: str = Field(..., description="Version of the model")
    execution_provider: str = Field(..., description="Execution provider of the model")
    device_type: DeviceType = Field(..., description="Device type of the model")
    uri: str = Field(..., description="URI of the model")
    file_size_mb: int = Field(..., description="Size of the model on disk in MB")
    supports_tool_calling: bool = Field(..., description="Whether the model supports tool calling")
    prompt_template: dict | None = Field(..., description="Prompt template for the model")
    provider: str = Field(..., description="Provider of the model")
    publisher: str = Field(..., description="Publisher of the model")
    license: str = Field(..., description="License of the model")
    task: str = Field(..., description="Task of the model")
    ep_override: str | None = Field(
        None, description="Override for the execution provider, if different from the model's default"
    )

    def __repr__(self) -> str:
        return (
            f"FoundryModelInfo(alias={self.alias}, id={self.id},"
            f" execution_provider={self.execution_provider}, device_type={self.device_type},"
            f" file_size={self.file_size_mb} MB, license={self.license})"
        )

    @classmethod
    def from_list_response(cls, response: dict | FoundryListResponseModel) -> FoundryModelInfo:
        """
        Create a FoundryModelInfo object from a FoundryListResponseModel object.

        Args:
            response (dict | FoundryListResponseModel): Response data.

        Returns:
            FoundryModelInfo: Instance of FoundryModelInfo.
        """
        if isinstance(response, dict):
            response = FoundryListResponseModel.model_validate(response)
        return cls(
            alias=response.alias,
            id=response.name,
            version=response.version,
            execution_provider=response.runtime.executionProvider,
            device_type=response.runtime.deviceType,
            uri=response.uri,
            file_size_mb=response.fileSizeMb,
            supports_tool_calling=response.supportsToolCalling,
            prompt_template=response.promptTemplate,
            provider=response.providerType,
            publisher=response.publisher,
            license=response.license,
            task=response.task,
        )

    def to_download_body(self) -> dict:
        """
        Convert the FoundryModelInfo object to a dictionary for download.

        Returns:
            dict: Dictionary representation for download.
        """
        return {
            "Name": self.id,
            "Uri": self.uri,
            "Publisher": self.publisher,
            "ProviderType": f"{self.provider}Local" if self.provider == "AzureFoundry" else self.provider,
            "PromptTemplate": self.prompt_template,
        }
