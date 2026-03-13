use std::collections::HashMap;

use crate::error::{FoundryLocalError, Result};

/// Log level for the Foundry Local service.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Fatal,
}

impl LogLevel {
    /// Returns the string value expected by the native core library.
    fn as_core_str(&self) -> &'static str {
        match self {
            Self::Trace => "Verbose",
            Self::Debug => "Debug",
            Self::Info => "Information",
            Self::Warn => "Warning",
            Self::Error => "Error",
            Self::Fatal => "Fatal",
        }
    }
}

/// User-facing configuration for initializing the Foundry Local SDK.
#[derive(Debug, Clone, Default)]
#[non_exhaustive]
pub struct FoundryLocalConfig {
    pub app_name: String,
    pub app_data_dir: Option<String>,
    pub model_cache_dir: Option<String>,
    pub logs_dir: Option<String>,
    pub log_level: Option<LogLevel>,
    pub web_service_urls: Option<String>,
    pub service_endpoint: Option<String>,
    pub library_path: Option<String>,
    pub additional_settings: Option<HashMap<String, String>>,
}

impl FoundryLocalConfig {
    /// Create a new configuration with the given application name.
    ///
    /// All other fields default to `None`. Use the struct update syntax to
    /// override specific options:
    ///
    /// ```ignore
    /// let config = FoundryLocalConfig {
    ///     log_level: Some(LogLevel::Debug),
    ///     ..FoundryLocalConfig::new("my_app")
    /// };
    /// ```
    pub fn new(app_name: impl Into<String>) -> Self {
        Self {
            app_name: app_name.into(),
            ..Self::default()
        }
    }
}

/// Internal configuration object that converts [`FoundryLocalConfig`] into the
/// parameter map expected by the native core library.
#[derive(Debug, Clone)]
pub(crate) struct Configuration {
    pub params: HashMap<String, String>,
}

impl Configuration {
    /// Build a [`Configuration`] from the user-facing [`FoundryLocalConfig`].
    ///
    /// # Errors
    ///
    /// Returns [`FoundryLocalError::InvalidConfiguration`] when `app_name` is
    /// empty or blank.
    pub fn new(config: FoundryLocalConfig) -> Result<Self> {
        let app_name = config.app_name.trim().to_string();
        if app_name.is_empty() {
            return Err(FoundryLocalError::InvalidConfiguration {
                reason: "app_name must be set and non-empty".into(),
            });
        }

        let mut params = HashMap::new();
        params.insert("AppName".into(), app_name);

        if let Some(v) = config.app_data_dir {
            params.insert("AppDataDir".into(), v);
        }
        if let Some(v) = config.model_cache_dir {
            params.insert("ModelCacheDir".into(), v);
        }
        if let Some(v) = config.logs_dir {
            params.insert("LogsDir".into(), v);
        }
        if let Some(level) = config.log_level {
            params.insert("LogLevel".into(), level.as_core_str().into());
        }
        if let Some(v) = config.web_service_urls {
            params.insert("WebServiceUrls".into(), v);
        }
        if let Some(v) = config.service_endpoint {
            params.insert("WebServiceExternalUrl".into(), v);
        }
        if let Some(v) = config.library_path {
            params.insert("FoundryLocalCorePath".into(), v);
        }
        if let Some(extra) = config.additional_settings {
            for (k, v) in extra {
                params.insert(k, v);
            }
        }

        Ok(Self { params })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn valid_config() {
        let cfg = FoundryLocalConfig {
            log_level: Some(LogLevel::Debug),
            ..FoundryLocalConfig::new("TestApp")
        };
        let c = Configuration::new(cfg).unwrap();
        assert_eq!(c.params["AppName"], "TestApp");
        assert_eq!(c.params["LogLevel"], "Debug");
    }

    #[test]
    fn empty_app_name_fails() {
        let cfg = FoundryLocalConfig {
            app_name: "  ".into(),
            ..FoundryLocalConfig::default()
        };
        assert!(Configuration::new(cfg).is_err());
    }
}
