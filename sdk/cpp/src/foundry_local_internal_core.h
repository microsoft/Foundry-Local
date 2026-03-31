// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string>
#include <string_view>
#include "logger.h"

namespace FoundryLocal {
    namespace Internal {
        struct IFoundryLocalCore {
            virtual ~IFoundryLocalCore() = default;

            virtual std::string call(std::string_view command, ILogger& logger,
                const std::string* dataArgument = nullptr, void* callback = nullptr,
                void* data = nullptr) const = 0;
            virtual void unload() = 0;
        };

    } // namespace Internal
} // namespace FoundryLocal