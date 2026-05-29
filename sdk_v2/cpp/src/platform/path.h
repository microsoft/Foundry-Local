// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <filesystem>
#include <string>

namespace fl::platform {

/// Cross-platform analogue of std::filesystem::weakly_canonical.
///
/// On POSIX this is a thin wrapper around std::filesystem::weakly_canonical.
///
/// On Windows it first attempts std::filesystem::weakly_canonical and, if that
/// fails with ERROR_ACCESS_DENIED (the AppContainer scenario — see
/// https://github.com/microsoft/onnxruntime/pull/28509), falls back to a manual
/// canonicalization that uses GetFinalPathNameByHandleW with
/// FILE_NAME_NORMALIZED | VOLUME_NAME_NT and prefixes the result with
/// "\\?\GLOBALROOT" so it remains a valid Win32 path. VOLUME_NAME_NT preserves
/// volume identity, so cross-volume containment checks still work — do NOT
/// switch to VOLUME_NAME_NONE.
///
/// Returns true on success. On failure populates `error_message` (best-effort,
/// for logging) and returns false.
bool GetWeaklyCanonicalPath(const std::filesystem::path& input,
                            std::filesystem::path& result,
                            std::string& error_message);

}  // namespace fl::platform
