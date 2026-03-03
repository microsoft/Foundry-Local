# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------

import logging
import re

from typing import Optional, Dict
from urllib.parse import urlparse

from src.exception import FoundryLocalException

logger = logging.getLogger(__name__)


class LogLevel:
    """Log level enumeration."""
    VERBOSE = "Verbose"
    DEBUG = "Debug"
    INFORMATION = "Information"
    WARNING = "Warning"
    ERROR = "Error"
    FATAL = "Fatal"


class Configuration:
    """Configuration for Foundry Local SDK.
    
    Configuration values:
        app_name: Your application name. MUST be set to a valid name.
        foundry_local_core_path: Path to the Foundry Local Core native library.
        app_data_dir: Application data directory.
            Default: {home}/.{appname}, where {home} is the user's home directory
                     and {appname} is the app_name value.
        model_cache_dir: Model cache directory.
            Default: {appdata}/cache/models, where {appdata} is the app_data_dir value.
        logs_dir: Log directory.
            Default: {appdata}/logs
        log_level: Logging level.
            Valid values are: Verbose, Debug, Information, Warning, Error, Fatal.
            Default: LogLevel.WARNING
        web: Optional configuration for the built-in web service.
            NOTE: This is not included in all builds.
        additional_settings: Additional settings that Foundry Local Core can consume.
            Keys and values are strings.
    """

    class WebService:
        """Configuration settings if the optional web service is used."""

        def __init__(
            self,
            urls: Optional[str] = None,
            external_url: Optional[str] = None
        ):
            """Initialize WebService configuration.
            
            Args:
                urls: Url/s to bind to the web service when 
                    FoundryLocalManager.start_web_service_async() is called.
                    After startup, FoundryLocalManager.urls will contain the actual URL/s 
                    the service is listening on.
                    Default: 127.0.0.1:0, which binds to a random ephemeral port.
                    Multiple URLs can be specified as a semi-colon separated list.
                external_url: If the web service is running in a separate process, 
                    it will be accessed using this URI.
                    Both processes should be using the same version of the SDK. 
                    If a random port is assigned when creating the web service in the 
                    external process the actual port must be provided here.
            """
            self.urls = urls
            self.external_url = external_url

    def __init__(
        self,
        app_name: str,
        foundry_local_core_path: Optional[str] = None,
        app_data_dir: Optional[str] = None,
        model_cache_dir: Optional[str] = None,
        logs_dir: Optional[str] = None,
        log_level: Optional[LogLevel] = LogLevel.WARNING,
        web: Optional['Configuration.WebService'] = None,
        additional_settings: Optional[Dict[str, str]] = None
    ):
        """Initialize Configuration.
        
        Args:
            app_name: Your application name. MUST be set to a valid name.
            app_data_dir: Application data directory. Optional.
            model_cache_dir: Model cache directory. Optional.
            logs_dir: Log directory. Optional.
            log_level: Logging level. Default: LogLevel.WARNING
            web: Optional configuration for the built-in web service.
            additional_settings: Additional settings dictionary. Optional.
        """
        self.app_name = app_name
        self.foundry_local_core_path = foundry_local_core_path
        self.app_data_dir = app_data_dir
        self.model_cache_dir = model_cache_dir
        self.logs_dir = logs_dir
        self.log_level = log_level
        self.web = web
        self.additional_settings = additional_settings

        # make sure app name only has safe characters as it's used as a directory name
        self._safe_app_name_chars = re.compile(r'^[A-Za-z0-9._-]+$')

    def validate(self) -> None:
        """Validate the configuration.
        
        Raises:
            FoundryLocalException: If configuration is invalid.
        """
        if not self.app_name:
            raise FoundryLocalException(
                "Configuration AppName must be set to a valid application name."
            )

        # Check for invalid filename characters
        if not bool(self._safe_app_name_chars.match(self.app_name)):
            raise FoundryLocalException("Configuration AppName value contains invalid characters.")

        if self.web is not None and self.web.external_url is not None:
            parsed = urlparse(self.web.external_url)
            if not parsed.port or parsed.port == 0:
                raise FoundryLocalException("Configuration Web.ExternalUrl has invalid port.")

    def as_dictionary(self) -> Dict[str, str]:
        """Convert configuration to a dictionary of string key-value pairs.
        
        Returns:
            Dictionary containing configuration values as strings.
            
        Raises:
            FoundryLocalException: If AppName is not set to a valid value.
        """
        if not self.app_name:
            raise FoundryLocalException(
                "Configuration AppName must be set to a valid application name."
            )

        config_values = {
            "AppName": self.app_name,
            "LogLevel": str(self.log_level)
        }

        if self.app_data_dir:
            config_values["AppDataDir"] = self.app_data_dir

        if self.model_cache_dir:
            config_values["ModelCacheDir"] = self.model_cache_dir

        if self.logs_dir:
            config_values["LogsDir"] = self.logs_dir

        if self.web is not None:
            if self.web.urls is not None:
                config_values["WebServiceUrls"] = self.web.urls

        # Emit any additional settings.
        if self.additional_settings is not None:
            for key, value in self.additional_settings.items():
                if not key:
                    continue  # skip empty keys
                config_values[key] = value if value is not None else ""

        return config_values
