# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
import os
from unittest import mock

import pytest
from foundry_local.api import FoundryLocalManager
from foundry_local.client import HttpResponseError
from foundry_local.models import FoundryModelInfo

# pylint: disable=unused-argument, redefined-outer-name

MOCK_INFO = {
    "providerType": "AzureFoundry",
    "version": "1",
    "modelType": "ONNX",
    "promptTemplate": {"prompt": "<|im_start|>user<|im_sep|>{Content}<|im_end|><|im_start|>assistant<|im_sep|>"},
    "publisher": "Microsoft",
    "task": "chat-completion",
    "fileSizeMb": 10403,
    "modelSettings": {"parameters": []},
    "supportsToolCalling": False,
    "license": "MIT",
    "licenseDescription": "This model is provided under the License Terms available at ...",
}

# Sample catalog with 3 aliases with different combos
MOCK_CATALOG_DATA = [
    # generic-gpu, generic-cpu
    {
        "name": "model-1-generic-gpu",
        "displayName": "model-1-generic-gpu",
        "uri": "azureml://registries/azureml/models/model-1-generic-gpu/versions/1",
        "runtime": {"deviceType": "GPU", "executionProvider": "WebGpuExecutionProvider"},
        "alias": "model-1",
        "parentModelUri": "azureml://registries/azureml/models/model-1/versions/1",
        **MOCK_INFO,
    },
    {
        "name": "model-1-generic-cpu",
        "displayName": "model-1-generic-cpu",
        "uri": "azureml://registries/azureml/models/model-1-generic-cpu/versions/1",
        "runtime": {"deviceType": "CPU", "executionProvider": "CPUExecutionProvider"},
        "alias": "model-1",
        "parentModelUri": "azureml://registries/azureml/models/model-1/versions/1",
        **MOCK_INFO,
    },
    # npu, generic-cpu
    {
        "name": "model-2-npu",
        "displayName": "model-2-npu",
        "uri": "azureml://registries/azureml/models/model-2-npu/versions/1",
        "runtime": {"deviceType": "NPU", "executionProvider": "QNNExecutionProvider"},
        "alias": "model-2",
        "parentModelUri": "azureml://registries/azureml/models/model-2/versions/1",
        **MOCK_INFO,
    },
    {
        "name": "model-2-generic-cpu",
        "displayName": "model-2-generic-cpu",
        "uri": "azureml://registries/azureml/models/model-2-generic-cpu/versions/1",
        "runtime": {"deviceType": "CPU", "executionProvider": "CPUExecutionProvider"},
        "alias": "model-2",
        "parentModelUri": "azureml://registries/azureml/models/model-2/versions/1",
        **MOCK_INFO,
    },
    # cuda-gpu, generic-gpu, generic-cpu
    {
        "name": "model-3-cuda-gpu",
        "displayName": "model-3-cuda-gpu",
        "uri": "azureml://registries/azureml/models/model-3-cuda-gpu/versions/1",
        "runtime": {"deviceType": "GPU", "executionProvider": "CUDAExecutionProvider"},
        "alias": "model-3",
        "parentModelUri": "azureml://registries/azureml/models/model-3/versions/1",
        **MOCK_INFO,
    },
    {
        "name": "model-3-generic-gpu",
        "displayName": "model-3-generic-gpu",
        "uri": "azureml://registries/azureml/models/model-3-generic-gpu/versions/1",
        "runtime": {"deviceType": "GPU", "executionProvider": "WebGpuExecutionProvider"},
        "alias": "model-3",
        "parentModelUri": "azureml://registries/azureml/models/model-3/versions/1",
        **MOCK_INFO,
    },
    {
        "name": "model-3-generic-cpu",
        "displayName": "model-3-generic-cpu",
        "uri": "azureml://registries/azureml/models/model-3-generic-cpu/versions/1",
        "runtime": {"deviceType": "CPU", "executionProvider": "CPUExecutionProvider"},
        "alias": "model-3",
        "parentModelUri": "azureml://registries/azureml/models/model-3/versions/1",
        **MOCK_INFO,
    },
    # generic-cpu
    {
        "name": "model-4-generic-gpu",
        "displayName": "model-4-generic-gpu",
        "uri": "azureml://registries/azureml/models/model-4-generic-gpu/versions/1",
        "runtime": {"deviceType": "GPU", "executionProvider": "WebGpuExecutionProvider"},
        "alias": "model-4",
        "parentModelUri": "azureml://registries/azureml/models/model-4/versions/1",
        **MOCK_INFO,
    },
]

# Mock response for /openai/status
MOCK_STATUS_RESPONSE = {"modelDirPath": "/test/path/to/models"}

# Mock response for /openai/models
MOCK_LOCAL_MODELS = ["model-2-npu", "model-4-generic-gpu"]

# Mock response for /openai/loadedmodels
MOCK_LOADED_MODELS = ["model-2-npu"]


@pytest.fixture(scope="module", autouse=True)
def mock_service_check():
    """Mock subprocess calls for service status check."""
    with mock.patch("subprocess.Popen") as mock_popen:
        mock_process = mock.MagicMock()
        mock_process.communicate.return_value = (b"Service is running at http://localhost:5272", b"")
        mock_popen.return_value.__enter__.return_value = mock_process
        yield mock_popen


@pytest.fixture(scope="module", autouse=True)
def mock_foundry_installed():
    """Mock check for foundry being installed."""
    with mock.patch("shutil.which") as mock_which:
        mock_which.return_value = "/usr/local/bin/foundry"
        yield mock_which


@pytest.fixture
def mock_http_client():
    """Mock HTTP client for API calls."""
    with mock.patch("foundry_local.api.HttpxClient", autospec=True) as mock_client:
        mock_instance = mock_client.return_value

        # Mock GET /foundry/list
        mock_instance.get.side_effect = lambda path, query_params=None: (
            MOCK_CATALOG_DATA
            if path == "/foundry/list"
            else (
                MOCK_STATUS_RESPONSE
                if path == "/openai/status"
                else (
                    MOCK_LOCAL_MODELS
                    if path == "/openai/models"
                    else MOCK_LOADED_MODELS
                    if path == "/openai/loadedmodels"
                    else None
                )
            )
        )

        # Mock POST /openai/download
        mock_instance.post_with_progress.return_value = {"success": True}

        yield mock_instance


def test_initialization(mock_http_client):
    """Test FoundryLocalManager initialization."""
    # Test with no bootstrap
    manager = FoundryLocalManager(bootstrap=False)
    assert manager.service_uri == "http://localhost:5272"
    assert manager.endpoint == "http://localhost:5272/v1"
    assert manager.api_key == (os.getenv("OPENAI_API_KEY") or "OPENAI_API_KEY")

    # Test with bootstrap and model_id
    with mock.patch("foundry_local.api.start_service") as mock_start:
        mock_start.return_value = "http://localhost:5272"
        manager = FoundryLocalManager(alias_or_model_id="model-2", bootstrap=True)
        mock_start.assert_called_once()
        mock_http_client.get.assert_any_call("/foundry/list")
        # in local models
        mock_http_client.post_with_progress.assert_not_called()


def test_list_catalog_models(mock_http_client):
    """Test listing catalog models."""
    manager = FoundryLocalManager(bootstrap=False)
    models = manager.list_catalog_models()
    mock_http_client.get.assert_called_once_with("/foundry/list")
    assert len(models) == 8
    assert all(isinstance(model, FoundryModelInfo) for model in models)
    assert [model.id for model in models] == [
        "model-1-generic-gpu",
        "model-1-generic-cpu",
        "model-2-npu",
        "model-2-generic-cpu",
        "model-3-cuda-gpu",
        "model-3-generic-gpu",
        "model-3-generic-cpu",
        "model-4-generic-gpu",
    ]
    assert [model.alias for model in models] == [
        "model-1",
        "model-1",
        "model-2",
        "model-2",
        "model-3",
        "model-3",
        "model-3",
        "model-4",
    ]

    # cached model catalog
    manager.list_catalog_models()
    mock_http_client.get.assert_called_once_with("/foundry/list")

    # refresh model catalog
    manager.refresh_catalog()
    manager.list_catalog_models()
    assert mock_http_client.get.call_count == 2


@pytest.mark.parametrize("platform", ["Windows", "Linux", "Darwin"])
def test_get_model_info(platform, mock_http_client):
    with mock.patch("platform.system", return_value=platform):
        manager = FoundryLocalManager(bootstrap=False)

        # unknown model
        # no raise
        assert manager.get_model_info("unknown-model") is None
        # raise
        with pytest.raises(ValueError):
            manager.get_model_info("unknown-model", raise_on_not_found=True)

        # with id
        assert manager.get_model_info("model-1-generic-cpu").id == "model-1-generic-cpu"

        # with alias
        # generic-cpu preferred on Windows
        assert (
            manager.get_model_info("model-1").id == "model-1-generic-cpu"
            if platform == "Windows"
            else "model-1-generic-gpu"
        )
        assert manager.get_model_info("model-2").id == "model-2-npu"
        assert manager.get_model_info("model-3").id == "model-3-cuda-gpu"


def test_list_cached_models(mock_http_client):
    """Test listing local models."""
    manager = FoundryLocalManager(bootstrap=False)
    local_models = manager.list_cached_models()
    assert len(local_models) == 2
    assert local_models[0].id == "model-2-npu"
    assert local_models[1].id == "model-4-generic-gpu"


def test_list_loaded_models(mock_http_client):
    """Test listing loaded models."""
    manager = FoundryLocalManager(bootstrap=False)
    loaded_models = manager.list_loaded_models()
    assert len(loaded_models) == 1
    assert loaded_models[0].id == "model-2-npu"


def test_download_model(mock_http_client):
    """Test downloading a model."""
    manager = FoundryLocalManager(bootstrap=False)

    # Test downloading a new model
    model_info = manager.download_model("model-3")
    assert model_info.id == "model-3-cuda-gpu"
    mock_http_client.post_with_progress.assert_called_once()

    # Reset mock for next test
    mock_http_client.post_with_progress.reset_mock()

    # Test downloading an already cached model
    model_info = manager.download_model("model-2")
    assert model_info.id == "model-2-npu"
    mock_http_client.post_with_progress.assert_not_called()

    # Test force download
    model_info = manager.download_model("model-2", force=True)
    assert model_info.id == "model-2-npu"
    mock_http_client.post_with_progress.assert_called_once()

    # Test download failure
    mock_http_client.post_with_progress.return_value = {"success": False, "errorMessage": "Download failed"}
    with pytest.raises(RuntimeError, match="Download failed"):
        manager.download_model("model-1")


def test_load_model(mock_http_client):
    """Test loading a model."""
    manager = FoundryLocalManager(bootstrap=False)

    # already loaded model
    model_info = manager.load_model("model-2")
    assert model_info.id == "model-2-npu"
    mock_http_client.get.assert_any_call("/openai/load/model-2-npu", query_params={"ttl": 600})

    # not loaded model
    model_info = manager.load_model("model-4")
    assert model_info.id == "model-4-generic-gpu"
    # ep override, should be cuda since there is cuda support
    mock_http_client.get.assert_any_call("/openai/load/model-4-generic-gpu", query_params={"ttl": 600, "ep": "cuda"})

    # Test loading a non-downloaded model
    def mock_get(path, query_params=None):
        raise HttpResponseError("No OpenAIService provider found for modelName")

    mock_http_client.get = mock_get
    with pytest.raises(ValueError, match="has not been downloaded yet"):
        manager.load_model("model-3")


def test_unload_model(mock_http_client):
    """Test unloading a model."""
    manager = FoundryLocalManager(bootstrap=False)

    # Test unloading a loaded model
    manager.unload_model("model-2")
    mock_http_client.get.assert_any_call("/openai/unload/model-2-npu", query_params={"force": False})

    # Test unloading a model that's not loaded
    mock_http_client.get.reset_mock()
    manager.unload_model("model-4")
    assert (
        mock.call("/openai/unload/model-4-generic-gpu", query_params={"force": False})
        not in mock_http_client.get.call_args_list
    )

    # Test force unloading
    manager.unload_model("model-2", force=True)
    mock_http_client.get.assert_any_call("/openai/unload/model-2-npu", query_params={"force": True})
