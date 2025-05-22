use serde::{Deserialize, Serialize};
use std::fmt;

/// Enumeration of devices supported by the model.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "UPPERCASE")]
pub enum DeviceType {
    CPU,
    GPU,
    NPU,
}

impl fmt::Display for DeviceType {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            DeviceType::CPU => write!(f, "CPU"),
            DeviceType::GPU => write!(f, "GPU"),
            DeviceType::NPU => write!(f, "NPU"),
        }
    }
}

/// Enumeration of execution providers supported by the model.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq, Hash)]
pub enum ExecutionProvider {
    #[serde(rename = "CPUExecutionProvider")]
    CPU,
    #[serde(rename = "WebGpuExecutionProvider")]
    WebGPU,
    #[serde(rename = "CUDAExecutionProvider")]
    CUDA,
    #[serde(rename = "QNNExecutionProvider")]
    QNN,
}

impl ExecutionProvider {
    /// Get the alias for the execution provider.
    pub fn get_alias(&self) -> String {
        match self {
            ExecutionProvider::CPU => "cpu".to_string(),
            ExecutionProvider::WebGPU => "webgpu".to_string(),
            ExecutionProvider::CUDA => "cuda".to_string(),
            ExecutionProvider::QNN => "qnn".to_string(),
        }
    }
}

impl fmt::Display for ExecutionProvider {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", match self {
            ExecutionProvider::CPU => "CPUExecutionProvider",
            ExecutionProvider::WebGPU => "WebGpuExecutionProvider",
            ExecutionProvider::CUDA => "CUDAExecutionProvider",
            ExecutionProvider::QNN => "QNNExecutionProvider",
        })
    }
}

/// Model runtime information.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ModelRuntime {
    #[serde(rename = "deviceType")]
    pub device_type: DeviceType,
    #[serde(rename = "executionProvider")]
    pub execution_provider: ExecutionProvider,
}

/// Response model for listing models.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FoundryListResponseModel {
    pub name: String,
    #[serde(rename = "displayName")]
    pub display_name: String,
    #[serde(rename = "modelType")]
    pub model_type: String,
    #[serde(rename = "providerType")]
    pub provider_type: String,
    pub uri: String,
    pub version: String,
    #[serde(rename = "promptTemplate")]
    pub prompt_template: serde_json::Value,
    pub publisher: String,
    pub task: String,
    pub runtime: ModelRuntime,
    #[serde(rename = "fileSizeMb")]
    pub file_size_mb: i32,
    #[serde(rename = "modelSettings")]
    pub model_settings: serde_json::Value,
    pub alias: String,
    #[serde(rename = "supportsToolCalling")]
    pub supports_tool_calling: bool,
    pub license: String,
    #[serde(rename = "licenseDescription")]
    pub license_description: String,
    #[serde(rename = "parentModelUri")]
    pub parent_model_uri: String,
}

/// Model information.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FoundryModelInfo {
    pub alias: String,
    pub id: String,
    pub version: String,
    pub runtime: ExecutionProvider,
    pub uri: String,
    pub file_size_mb: i32,
    pub prompt_template: serde_json::Value,
    pub provider: String,
    pub publisher: String,
    pub license: String,
    pub task: String,
}

impl FoundryModelInfo {
    /// Create a FoundryModelInfo object from a FoundryListResponseModel object.
    pub fn from_list_response(response: &FoundryListResponseModel) -> Self {
        Self {
            alias: response.alias.clone(),
            id: response.name.clone(),
            version: response.version.clone(),
            runtime: response.runtime.execution_provider.clone(),
            uri: response.uri.clone(),
            file_size_mb: response.file_size_mb,
            prompt_template: response.prompt_template.clone(),
            provider: response.provider_type.clone(),
            publisher: response.publisher.clone(),
            license: response.license.clone(),
            task: response.task.clone(),
        }
    }

    /// Convert the FoundryModelInfo object to a dictionary for download.
    pub fn to_download_body(&self) -> serde_json::Value {
        let provider_type = if self.provider == "AzureFoundry" {
            format!("{}Local", self.provider)
        } else {
            self.provider.clone()
        };

        serde_json::json!({
            "model": {
                "Name": self.id,
                "Uri": self.uri,
                "Publisher": self.publisher,
                "ProviderType": provider_type,
                "PromptTemplate": self.prompt_template,
            },
            "IgnorePipeReport": true
        })
    }
}

impl fmt::Display for FoundryModelInfo {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "FoundryModelInfo(alias={}, id={}, runtime={}, file_size={} MB, license={})",
            self.alias, self.id, self.runtime.get_alias(), self.file_size_mb, self.license)
    }
} 