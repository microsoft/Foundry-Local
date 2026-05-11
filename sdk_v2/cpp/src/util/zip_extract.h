// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <filesystem>

namespace fl {

/// Extract a ZIP archive to a directory.
/// Creates the destination directory if it doesn't exist.
/// @return true on success.
bool ExtractZip(const std::filesystem::path& zip_path,
                const std::filesystem::path& destination);

}  // namespace fl
