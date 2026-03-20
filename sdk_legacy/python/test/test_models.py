# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
from foundry_local.models import DeviceType, ExecutionProvider, FoundryListResponseModel, FoundryModelInfo, ModelRuntime


def test_device_type_enum():
    """Test DeviceType enum."""
    assert DeviceType.CPU == "CPU"
    assert DeviceType.GPU == "GPU"
    assert DeviceType.NPU == "NPU"
    assert str(DeviceType.CPU) == "CPU"


def test_execution_provider_enum():
    """Test ExecutionProvider enum."""
    assert ExecutionProvider.CPU == "CPUExecutionProvider"
    assert ExecutionProvider.WEBGPU == "WebGpuExecutionProvider"
    assert ExecutionProvider.CUDA == "CUDAExecutionProvider"

    # Test get_alias method
    assert ExecutionProvider.CPU.get_alias() == "cpu"
    assert ExecutionProvider.WEBGPU.get_alias() == "webgpu"
    assert ExecutionProvider.CUDA.get_alias() == "cuda"


def test_model_runtime():
    """Test ModelRuntime class."""
    runtime = ModelRuntime(deviceType=DeviceType.CPU, executionProvider=ExecutionProvider.CPU)
    assert runtime.deviceType == DeviceType.CPU
    assert runtime.executionProvider == ExecutionProvider.CPU


def test_foundry_list_response_model():
    """Test FoundryListResponseModel class."""
    response_data = {
        "name": "test-model",
        "displayName": "Test Model",
        "modelType": "ONNX",
        "providerType": "AzureFoundry",
        "uri": "azureml://test",
        "version": "1",
        "promptTemplate": {"prompt": "test"},
        "publisher": "Test",
        "task": "chat-completion",
        "runtime": {"deviceType": "CPU", "executionProvider": "CPUExecutionProvider"},
        "fileSizeMb": 1000,
        "modelSettings": {},
        "alias": "test",
        "supportsToolCalling": False,
        "license": "MIT",
        "licenseDescription": "Test license",
        "parentModelUri": "azureml://parent",
        "maxOutputTokens": 1024,
        "minFLVersion": "1.0.0",
    }

    model = FoundryListResponseModel.model_validate(response_data)
    assert model.name == "test-model"
    assert model.displayName == "Test Model"
    assert model.modelType == "ONNX"
    assert model.providerType == "AzureFoundry"
    assert model.uri == "azureml://test"
    assert model.version == "1"
    assert model.promptTemplate == {"prompt": "test"}
    assert model.publisher == "Test"
    assert model.task == "chat-completion"
    assert model.runtime.deviceType == DeviceType.CPU
    assert model.runtime.executionProvider == ExecutionProvider.CPU
    assert model.fileSizeMb == 1000
    assert model.modelSettings == {}
    assert model.alias == "test"
    assert model.supportsToolCalling is False
    assert model.license == "MIT"
    assert model.licenseDescription == "Test license"
    assert model.parentModelUri == "azureml://parent"


def test_foundry_model_info():
    """Test FoundryModelInfo class."""
    model_info = FoundryModelInfo(
        alias="test",
        id="test-model",
        version="1",
        execution_provider=ExecutionProvider.CPU,
        device_type=DeviceType.CPU,
        uri="azureml://test",
        file_size_mb=1000,
        supports_tool_calling=False,
        prompt_template={"prompt": "test"},
        provider="AzureFoundry",
        publisher="Test",
        license="MIT",
        task="chat-completion",
    )

    assert model_info.alias == "test"
    assert model_info.id == "test-model"
    assert model_info.version == "1"
    assert model_info.execution_provider == ExecutionProvider.CPU
    assert model_info.device_type == DeviceType.CPU
    assert model_info.uri == "azureml://test"
    assert model_info.file_size_mb == 1000
    assert model_info.prompt_template == {"prompt": "test"}
    assert model_info.provider == "AzureFoundry"
    assert model_info.publisher == "Test"
    assert model_info.license == "MIT"
    assert model_info.task == "chat-completion"

    # Test __repr__
    repr_str = repr(model_info)
    assert "alias=test" in repr_str
    assert "id=test-model" in repr_str
    assert "provider=CPUExecutionProvider" in repr_str
    assert "device_type=CPU" in repr_str
    assert "file_size=1000 MB" in repr_str
    assert "license=MIT" in repr_str


def test_from_list_response():
    """Test from_list_response class method."""
    response_data = {
        "name": "test-model",
        "displayName": "Test Model",
        "modelType": "ONNX",
        "providerType": "AzureFoundry",
        "uri": "azureml://test",
        "version": "1",
        "promptTemplate": {"prompt": "test"},
        "publisher": "Test",
        "task": "chat-completion",
        "runtime": {"deviceType": "CPU", "executionProvider": "CPUExecutionProvider"},
        "fileSizeMb": 1000,
        "modelSettings": {},
        "alias": "test",
        "supportsToolCalling": False,
        "license": "MIT",
        "licenseDescription": "Test license",
        "parentModelUri": "azureml://parent",
        "maxOutputTokens": 1024,
        "minFLVersion": "1.0.0",
    }

    # Test with dict
    model_info = FoundryModelInfo.from_list_response(response_data)
    assert model_info.alias == "test"
    assert model_info.id == "test-model"
    assert model_info.version == "1"
    assert model_info.execution_provider == ExecutionProvider.CPU
    assert model_info.device_type == DeviceType.CPU
    assert model_info.uri == "azureml://test"
    assert model_info.file_size_mb == 1000
    assert model_info.prompt_template == {"prompt": "test"}
    assert model_info.provider == "AzureFoundry"
    assert model_info.publisher == "Test"
    assert model_info.license == "MIT"
    assert model_info.task == "chat-completion"

    # Test with FoundryListResponseModel instance
    model = FoundryListResponseModel.model_validate(response_data)
    model_info = FoundryModelInfo.from_list_response(model)
    assert model_info.alias == "test"
    assert model_info.id == "test-model"
    assert model_info.version == "1"
    assert model_info.execution_provider == ExecutionProvider.CPU
    assert model_info.device_type == DeviceType.CPU
    assert model_info.uri == "azureml://test"
    assert model_info.file_size_mb == 1000
    assert model_info.prompt_template == {"prompt": "test"}
    assert model_info.provider == "AzureFoundry"
    assert model_info.publisher == "Test"
    assert model_info.license == "MIT"
    assert model_info.task == "chat-completion"


def test_to_download_body():
    """Test to_download_body method."""
    model_info = FoundryModelInfo(
        alias="test",
        id="test-model",
        version="1",
        execution_provider=ExecutionProvider.CPU,
        device_type=DeviceType.CPU,
        uri="azureml://test",
        file_size_mb=1000,
        supports_tool_calling=False,
        prompt_template={"prompt": "test"},
        provider="AzureFoundry",
        publisher="Test",
        license="MIT",
        task="chat-completion",
    )

    download_body = model_info.to_download_body()
    assert download_body["Name"] == "test-model"
    assert download_body["Uri"] == "azureml://test"
    assert download_body["Publisher"] == "Test"
    assert download_body["ProviderType"] == "AzureFoundryLocal"  # Note the "Local" suffix
    assert download_body["PromptTemplate"] == {"prompt": "test"}

    # Test with different provider
    model_info.provider = "OtherProvider"
    download_body = model_info.to_download_body()
    assert download_body["ProviderType"] == "OtherProvider"  # No "Local" suffix for other providers
