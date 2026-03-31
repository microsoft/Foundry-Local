// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <stdexcept>
#include <string>

#include "logger.h"

namespace FoundryLocal {

    class FoundryLocalException final : public std::runtime_error {
    public:
        explicit FoundryLocalException(std::string message) : std::runtime_error(std::move(message)) {}

        FoundryLocalException(std::string message, ILogger& logger) : std::runtime_error(std::move(message)) {
            logger.Log(LogLevel::Error, what());
        }
    };

} // namespace FoundryLocal
