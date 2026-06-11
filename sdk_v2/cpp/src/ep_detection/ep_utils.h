// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>

namespace fl {

class ILogger;

/// Verify a set of binaries in @p dir all exist and match their expected SHA-256 hashes.
///
/// @param dir            Directory containing the extracted EP binaries.
/// @param expected       Map of filename -> expected_sha256_hex.
/// @param ep_name        EP name used in warning log messages (e.g. "CUDA EP (ort)").
/// @param logger         Logger for diagnostic output.
/// @return true if every file exists and its hash matches; false otherwise.
bool VerifyEpPackage(
    const std::filesystem::path& dir,
    const std::unordered_map<std::string, std::string>& expected,
    std::string_view ep_name,
    ILogger& logger);

}  // namespace fl
