// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <filesystem>
#include <initializer_list>
#include <string_view>
#include <utility>

namespace fl {

class ILogger;

/// Verify a set of binaries in @p dir all exist and match their expected SHA-256 hashes.
///
/// @param dir            Directory containing the extracted EP binaries.
/// @param expected       List of (filename, expected_sha256_hex) pairs.
/// @param ep_name        EP name used in warning log messages (e.g. "CUDA EP").
/// @param logger         Logger for diagnostic output.
/// @return true if every file exists and its hash matches; false otherwise.
bool VerifyEpPackage(
    const std::filesystem::path& dir,
    std::initializer_list<std::pair<std::string_view, std::string_view>> expected,
    std::string_view ep_name,
    ILogger& logger);

/// Prepend @p dir to the process `PATH` environment variable for the lifetime of the process.
///
/// EP provider libraries (CUDA, WebGPU) delay-load sibling dependency DLLs from their own directory,
/// and `RegisterExecutionProviderLibrary` loads the provider DLL eagerly. The directory must be on
/// `PATH` before registration so those dependencies are discoverable. This is a no-op on non-Windows
/// platforms.
///
/// @param dir Directory to prepend to `PATH`.
void PrependDirToProcessPath(const std::filesystem::path& dir);

}  // namespace fl
