#pragma once
#include <string_view>
#include "log_level.h"

namespace FoundryLocal {
    class ILogger {
    public:
        virtual ~ILogger() = default;
        virtual void Log(LogLevel level, std::string_view message) noexcept = 0;
    };

    class NullLogger final : public ILogger {
    public:
        void Log(LogLevel, std::string_view) noexcept override {}
    };
} // namespace FoundryLocal
