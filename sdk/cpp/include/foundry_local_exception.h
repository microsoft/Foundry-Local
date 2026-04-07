// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <stdexcept>
#include <string>

#include "logger.h"

namespace foundry_local {

    class Exception final : public std::runtime_error {
    public:
        explicit Exception(std::string message) : std::runtime_error(std::move(message)) {}

        Exception(std::string message, ILogger& logger) : std::runtime_error(std::move(message)) {
            logger.Log(LogLevel::Error, what());
        }
    };
} // namespace foundry_local
