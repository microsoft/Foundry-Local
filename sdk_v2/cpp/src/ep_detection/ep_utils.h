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

/// Extract the runtime-native payload from a NuGet package into @p destination.
///
/// The package is extracted to a temporary subdirectory, then files from
/// `runtimes/{rid}/native/` are copied into @p destination.
bool ExtractNuGetRuntimePayload(const std::filesystem::path& package_path,
                                std::string_view rid,
                                const std::filesystem::path& destination,
                                ILogger& logger);

/// Verify that a NuGet package carries a Microsoft author signature.
bool VerifyMicrosoftNuGetAuthorSignature(const std::filesystem::path& package_path,
                                         ILogger& logger);

}  // namespace fl
