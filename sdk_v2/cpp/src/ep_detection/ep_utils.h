// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

namespace fl {

class ILogger;

/// Extract a NuGet package once, verify Microsoft author signature, then copy
/// `runtimes/{rid}/native/` payload files into @p destination.
bool VerifyPackage(const std::filesystem::path& package_path,
                   std::string_view rid,
                   const std::filesystem::path& destination,
                   ILogger& logger);

}  // namespace fl
