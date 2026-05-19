// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include "logger.h"

#ifndef FL_CDECL
  #ifdef _WIN32
    #define FL_CDECL __cdecl
  #else
    #define FL_CDECL
  #endif
#endif

namespace foundry_local {

    /// Native callback signature used by the core DLL interop.
    /// Parameters: (data, dataLength, userData).
    /// Return 0 to continue, 1 to cancel the native operation.
    using NativeCallbackFn = int32_t(FL_CDECL*)(const void*, int32_t, void*);

    /// Value returned by IFoundryLocalCore::call().
    /// On success, `data` contains the response payload and `error` is empty.
    /// On failure, `error` contains the error message from the core layer.
    struct CoreResponse {
        std::string data;
        std::string error;

        bool HasError() const noexcept { return !error.empty(); }
    };

    namespace Internal {
        struct IFoundryLocalCore {
            virtual ~IFoundryLocalCore() = default;

            virtual CoreResponse call(std::string_view command, ILogger& logger,
                                       const std::string* dataArgument = nullptr, NativeCallbackFn callback = nullptr,
                                       void* data = nullptr,
                                       std::function<bool()> isCancellationRequested = nullptr) const = 0;

            virtual CoreResponse callWithBinary(std::string_view command, ILogger& logger,
                                                const std::string* dataArgument,
                                                const uint8_t* binaryData, size_t binaryDataLength) const = 0;

            virtual void unload() = 0;
        };

    } // namespace Internal
} // namespace foundry_local
