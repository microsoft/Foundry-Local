// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string_view>

namespace FoundryLocal {

    enum class LogLevel {
        Verbose,
        Debug,
        Information,
        Warning,
        Error,
        Fatal
    };

    inline std::string_view LogLevelToString(LogLevel level) noexcept {
        switch (level) {
        case LogLevel::Verbose:
            return "Verbose";
        case LogLevel::Debug:
            return "Debug";
        case LogLevel::Information:
            return "Information";
        case LogLevel::Warning:
            return "Warning";
        case LogLevel::Error:
            return "Error";
        case LogLevel::Fatal:
            return "Fatal";
        }
        return "Unknown";
    }

} // namespace FoundryLocal
