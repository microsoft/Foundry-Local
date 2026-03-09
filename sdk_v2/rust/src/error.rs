use thiserror::Error;

/// Errors that can occur when using the Foundry Local SDK.
#[derive(Debug, Error)]
pub enum FoundryLocalError {
    /// The native core library could not be loaded.
    #[error("library load error: {0}")]
    LibraryLoad(String),
    /// A command executed against the native core returned an error.
    #[error("command execution error: {0}")]
    CommandExecution(String),
    /// The provided configuration is invalid.
    #[error("invalid configuration: {0}")]
    InvalidConfiguration(String),
    /// A model operation failed (load, unload, download, etc.).
    #[error("model operation error: {0}")]
    ModelOperation(String),
    /// An HTTP request to the external service failed.
    #[error("HTTP request error: {0}")]
    HttpRequest(#[from] reqwest::Error),
    /// Serialization or deserialization of JSON data failed.
    #[error("serialization error: {0}")]
    Serialization(#[from] serde_json::Error),
    /// A validation check on user-supplied input failed.
    #[error("validation error: {0}")]
    Validation(String),
    /// An I/O error occurred.
    #[error("I/O error: {0}")]
    Io(#[from] std::io::Error),
}

/// Convenience alias used throughout the SDK.
pub type Result<T> = std::result::Result<T, FoundryLocalError>;
