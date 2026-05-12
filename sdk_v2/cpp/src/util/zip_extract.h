// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <filesystem>
#include <string_view>

namespace fl {

class ILogger;

/// Returns true if `entry` is a safe archive entry path — i.e. extracting it
/// will not escape the destination directory ("zip-slip" defense). Rejects:
///   * any entry with a `..` path component (split on '/' or '\\')
///   * absolute POSIX paths (starting with '/')
///   * Windows absolute paths (drive-letter `X:` prefix or leading '\\')
/// Used as a pre-validation pass before invoking the system tar extractor.
bool IsSafeArchiveEntry(std::string_view entry);

/// Extract a ZIP archive to a directory.
/// Creates the destination directory if it doesn't exist.
/// Performs a zip-slip pre-validation pass over the archive's entry list and
/// refuses to extract if any entry would escape `destination`.
/// Diagnostic messages for any failure (spawn error, non-zero tar exit,
/// unsafe entry, etc.) are emitted via `logger` so production failures are
/// debuggable from the SDK log.
/// @return true on success.
bool ExtractZip(const std::filesystem::path& zip_path,
                const std::filesystem::path& destination,
                ILogger& logger);

}  // namespace fl
