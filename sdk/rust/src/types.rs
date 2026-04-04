use serde::{Deserialize, Serialize, Serializer};

/// Hardware device type for model execution.
#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum DeviceType {
    Invalid,
    #[default]
    CPU,
    GPU,
    NPU,
}

/// Prompt template describing how messages are formatted for the model.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct PromptTemplate {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub system: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub user: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub assistant: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub prompt: Option<String>,
}

/// Runtime information for a model (device type and execution provider).
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct Runtime {
    pub device_type: DeviceType,
    pub execution_provider: String,
}

/// A single parameter key-value pair used in model settings.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct Parameter {
    pub name: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub value: Option<String>,
}

/// Model-level settings containing a list of parameters.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct ModelSettings {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub parameters: Option<Vec<Parameter>>,
}

/// Full metadata for a model variant as returned by the catalog.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct ModelInfo {
    pub id: String,
    pub name: String,
    pub version: u64,
    pub alias: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub display_name: Option<String>,
    pub provider_type: String,
    pub uri: String,
    pub model_type: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub prompt_template: Option<PromptTemplate>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub publisher: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub model_settings: Option<ModelSettings>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub license: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub license_description: Option<String>,
    pub cached: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub task: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub runtime: Option<Runtime>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub file_size_mb: Option<u64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub supports_tool_calling: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub max_output_tokens: Option<u64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub min_fl_version: Option<String>,
    #[serde(default)]
    pub created_at_unix: u64,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub context_length: Option<u64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub input_modalities: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub output_modalities: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub capabilities: Option<String>,
}

/// Desired response format for chat completions.
///
/// Extends the standard OpenAI formats with the Foundry-specific
/// `LarkGrammar` variant.
#[derive(Debug, Clone)]
pub enum ChatResponseFormat {
    /// Plain text output (default).
    Text,
    /// JSON output (unstructured).
    JsonObject,
    /// JSON output constrained by a schema string.
    JsonSchema(String),
    /// Output constrained by a Lark grammar (Foundry extension).
    LarkGrammar(String),
}

impl Serialize for ChatResponseFormat {
    fn serialize<S: Serializer>(&self, serializer: S) -> std::result::Result<S::Ok, S::Error> {
        use serde::ser::SerializeMap;
        match self {
            ChatResponseFormat::Text => {
                let mut map = serializer.serialize_map(Some(1))?;
                map.serialize_entry("type", "text")?;
                map.end()
            }
            ChatResponseFormat::JsonObject => {
                let mut map = serializer.serialize_map(Some(1))?;
                map.serialize_entry("type", "json_object")?;
                map.end()
            }
            ChatResponseFormat::JsonSchema(schema) => {
                let mut map = serializer.serialize_map(Some(2))?;
                map.serialize_entry("type", "json_schema")?;
                map.serialize_entry("json_schema", schema)?;
                map.end()
            }
            ChatResponseFormat::LarkGrammar(grammar) => {
                let mut map = serializer.serialize_map(Some(2))?;
                map.serialize_entry("type", "lark_grammar")?;
                map.serialize_entry("lark_grammar", grammar)?;
                map.end()
            }
        }
    }
}

/// Tool choice configuration for chat completions.
#[derive(Debug, Clone)]
pub enum ChatToolChoice {
    /// Model will not call any tool.
    None,
    /// Model decides whether to call a tool.
    Auto,
    /// Model must call at least one tool.
    Required,
    /// Model must call the named function.
    Function(String),
}

impl Serialize for ChatToolChoice {
    fn serialize<S: Serializer>(&self, serializer: S) -> std::result::Result<S::Ok, S::Error> {
        use serde::ser::SerializeMap;
        match self {
            ChatToolChoice::None => serializer.serialize_str("none"),
            ChatToolChoice::Auto => serializer.serialize_str("auto"),
            ChatToolChoice::Required => serializer.serialize_str("required"),
            ChatToolChoice::Function(name) => {
                let mut map = serializer.serialize_map(Some(2))?;
                map.serialize_entry("type", "function")?;
                let func = serde_json::json!({ "name": name });
                map.serialize_entry("function", &func)?;
                map.end()
            }
        }
    }
}

/// Information about an available execution provider bootstrapper.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "PascalCase")]
pub struct EpInfo {
    /// The name of the execution provider.
    pub name: String,
    /// Whether this EP is currently registered and ready for use.
    pub is_registered: bool,
}

/// Result of a download-and-register execution-provider operation.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "PascalCase")]
pub struct EpDownloadResult {
    /// Whether all requested EPs were successfully registered.
    pub success: bool,
    /// Human-readable status message.
    pub status: String,
    /// Names of EPs that were successfully registered.
    pub registered_eps: Vec<String>,
    /// Names of EPs that failed to register.
    pub failed_eps: Vec<String>,
}
