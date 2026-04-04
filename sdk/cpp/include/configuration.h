// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include <string>
#include <optional>
#include <unordered_map>
#include <stdexcept>
#include <filesystem>
#include "log_level.h"

namespace foundry_local {

    /// Optional configuration for the built-in web service.
    struct WebServiceConfig {
        // URL/s to bind the web service to.
        // Default: 127.0.0.1:0 (random ephemeral port).
        // Multiple URLs can be specified as a semicolon-separated list.
        std::optional<std::string> urls;

        // If the web service is running in a separate process, provide its URL here.
        std::optional<std::string> external_url;
    };

    struct Configuration {
        // Construct a Configuration with just an application name.
        // All other fields use their defaults.
        Configuration(std::string name) : app_name(std::move(name)) {}

        // Your application name. MUST be set to a valid name.
        std::string app_name;

        // Application data directory.
        // Default: {home}/.{appname}, where {home} is the user's home directory and {appname} is the app_name value.
        std::optional<std::filesystem::path> app_data_dir;

        // Model cache directory.
        // Default: {appdata}/cache/models, where {appdata} is the app_data_dir value.
        std::optional<std::filesystem::path> model_cache_dir;

        // Log directory.
        // Default: {appdata}/logs
        std::optional<std::filesystem::path> logs_dir;

        // Logging level.
        // Valid values are: Verbose, Debug, Information, Warning, Error, Fatal.
        // Default: LogLevel.Warning
        LogLevel log_level = LogLevel::Warning;

        // Optional web service configuration.
        std::optional<WebServiceConfig> web;

        // Additional settings that Foundry Local Core can consume.
        std::optional<std::unordered_map<std::string, std::string>> additional_settings;

        void Validate() const {
            if (app_name.empty()) {
                throw std::invalid_argument("Configuration app_name must be set to a valid application name.");
            }

            constexpr std::string_view invalidChars = R"(\/:?\"<>|)";
            if (app_name.find_first_of(invalidChars) != std::string::npos) {
                throw std::invalid_argument("Configuration app_name value contains invalid characters.");
            }
        }
    };

} // namespace foundry_local
