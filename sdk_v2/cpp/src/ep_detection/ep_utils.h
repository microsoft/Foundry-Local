// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "logger.h"
#include "util/sha256.h"

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

namespace fl {

/// Verify a set of binaries in @p dir all exist and match their expected SHA-256 hashes.
///
/// @param dir            Directory containing the extracted EP binaries.
/// @param expected       List of (filename, expected_sha256_hex) pairs.
/// @param ep_name        EP name used in warning log messages (e.g. "CUDA EP").
/// @param logger         Logger for diagnostic output.
/// @return true if every file exists and its hash matches; false otherwise.
inline bool VerifyEpPackage(
    const std::filesystem::path& dir,
    std::initializer_list<std::pair<std::string_view, std::string_view>> expected,
    std::string_view ep_name,
    ILogger& logger) {
  for (const auto& [filename, expected_hash] : expected) {
    auto file_path = dir / filename;

    if (!std::filesystem::exists(file_path)) {
      return false;
    }

    auto hash = Sha256File(file_path);

    // Case-insensitive hex comparison
    if (!std::equal(hash.begin(), hash.end(), expected_hash.begin(), expected_hash.end(),
                    [](char a, char b) { return std::toupper(a) == std::toupper(b); })) {
      logger.Log(LogLevel::Warning,
                 fmt::format("{}: hash mismatch for {}: got {}, expected {}",
                             ep_name, filename, hash, expected_hash));
      return false;
    }
  }

  return true;
}

}  // namespace fl
