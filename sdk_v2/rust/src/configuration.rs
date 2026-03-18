use std::collections::HashMap;
use std::fmt;

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

/// Application-level logger that the SDK can use to emit diagnostic messages.
///
/// This is a stub — the logger is stored in the configuration and passed
/// through to the manager, but it is not wired into the native core yet.
pub trait Logger: Send + Sync {
    /// Log a message at the given severity level.
    fn log(&self, level: LogLevel, message: &str);
}

/// User-facing configuration for initializing the Foundry Local SDK.
///
/// Construct with [`FoundryLocalConfig::new`] and customise via the builder
/// methods:
///
/// ```ignore
/// let config = FoundryLocalConfig::new("my_app")
///     .log_level(LogLevel::Debug)
///     .model_cache_dir("/path/to/cache");
/// ```
#[derive(Default)]
pub struct FoundryLocalConfig {
    app_name: String,
    app_data_dir: Option<String>,
    model_cache_dir: Option<String>,
    logs_dir: Option<String>,
    log_level: Option<LogLevel>,
    web_service_urls: Option<String>,
    service_endpoint: Option<String>,
    library_path: Option<String>,
    additional_settings: Option<HashMap<String, String>>,
    logger: Option<Box<dyn Logger>>,
}

impl fmt::Debug for FoundryLocalConfig {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("FoundryLocalConfig")
            .field("app_name", &self.app_name)
            .field("app_data_dir", &self.app_data_dir)
            .field("model_cache_dir", &self.model_cache_dir)
            .field("logs_dir", &self.logs_dir)
            .field("log_level", &self.log_level)
            .field("web_service_urls", &self.web_service_urls)
            .field("service_endpoint", &self.service_endpoint)
            .field("library_path", &self.library_path)
            .field("additional_settings", &self.additional_settings)
            .field("logger", &self.logger.as_ref().map(|_| ".."))
            .finish()
    }
}

impl FoundryLocalConfig {
    /// Create a new configuration with the given application name.
    ///
    /// All other fields default to `None`. Use the builder methods to
    /// customise:
    ///
    /// ```ignore
    /// let config = FoundryLocalConfig::new("my_app")
    ///     .log_level(LogLevel::Debug)
    ///     .model_cache_dir("/path/to/cache");
    /// ```
    pub fn new(app_name: impl Into<String>) -> Self {
        Self {
            app_name: app_name.into(),
            ..Self::default()
        }
    }

    /// Override the application-data directory.
    pub fn app_data_dir(mut self, dir: impl Into<String>) -> Self {
        self.app_data_dir = Some(dir.into());
        self
    }

    /// Override the model-cache directory.
    pub fn model_cache_dir(mut self, dir: impl Into<String>) -> Self {
        self.model_cache_dir = Some(dir.into());
        self
    }

    /// Override the logs directory.
    pub fn logs_dir(mut self, dir: impl Into<String>) -> Self {
        self.logs_dir = Some(dir.into());
        self
    }

    /// Set the log level.
    pub fn log_level(mut self, level: LogLevel) -> Self {
        self.log_level = Some(level);
        self
    }

    /// Set the web-service listen URLs (e.g. `"http://localhost:5273"`).
    pub fn web_service_urls(mut self, urls: impl Into<String>) -> Self {
        self.web_service_urls = Some(urls.into());
        self
    }

    /// Set an external service endpoint URL.
    pub fn service_endpoint(mut self, endpoint: impl Into<String>) -> Self {
        self.service_endpoint = Some(endpoint.into());
        self
    }

    /// Override the path to the native Foundry Local Core library.
    pub fn library_path(mut self, path: impl Into<String>) -> Self {
        self.library_path = Some(path.into());
        self
    }

    /// Add a single key-value pair to the additional settings map.
    pub fn additional_setting(mut self, key: impl Into<String>, value: impl Into<String>) -> Self {
        self.additional_settings
            .get_or_insert_with(HashMap::new)
            .insert(key.into(), value.into());
        self
    }

    /// Provide an application logger.
    ///
    /// *Stub* — the logger is stored but not yet wired into the native core.
    pub fn logger(mut self, logger: impl Logger + 'static) -> Self {
        self.logger = Some(Box::new(logger));
        self
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
    /// Returns the parameter map **and** the optional logger extracted from the
    /// config (since the logger cannot be represented as a string parameter).
    ///
    /// # Errors
    ///
    /// Returns [`FoundryLocalError::InvalidConfiguration`] when `app_name` is
    /// empty or blank.
    pub fn new(config: FoundryLocalConfig) -> Result<(Self, Option<Box<dyn Logger>>)> {
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

        Ok((Self { params }, config.logger))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn valid_config() {
        let cfg = FoundryLocalConfig::new("TestApp")
            .log_level(LogLevel::Debug);
        let (c, _) = Configuration::new(cfg).unwrap();
        assert_eq!(c.params["AppName"], "TestApp");
        assert_eq!(c.params["LogLevel"], "Debug");
    }

    #[test]
    fn empty_app_name_fails() {
        let cfg = FoundryLocalConfig::new("  ");
        assert!(Configuration::new(cfg).is_err());
    }

    #[test]
    fn builder_additional_settings() {
        let cfg = FoundryLocalConfig::new("App")
            .additional_setting("Bootstrap", "false")
            .additional_setting("Foo", "bar");
        let (c, _) = Configuration::new(cfg).unwrap();
        assert_eq!(c.params["Bootstrap"], "false");
        assert_eq!(c.params["Foo"], "bar");
    }
}
