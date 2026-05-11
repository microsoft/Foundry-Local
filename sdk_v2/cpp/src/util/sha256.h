// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <filesystem>
#include <string>

namespace fl {

/// Compute SHA256 hash of a file and return it as uppercase hex string.
/// Returns empty string on error.
std::string Sha256File(const std::filesystem::path& file_path);

}  // namespace fl
