// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include <string_view>
#include "log_level.h"

namespace foundry_local {
    class ILogger {
    public:
        virtual ~ILogger() = default;
        virtual void Log(LogLevel level, std::string_view message) noexcept = 0;
    };

    class NullLogger final : public ILogger {
    public:
        void Log(LogLevel, std::string_view) noexcept override {}
    };
} // namespace foundry_local
