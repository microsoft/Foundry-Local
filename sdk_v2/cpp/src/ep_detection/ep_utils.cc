// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "ep_detection/ep_utils.h"

#include "logger.h"
#include "util/sha256.h"

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <string>

namespace fl {

bool VerifyEpPackage(
    const std::filesystem::path& dir,
    const std::unordered_map<std::string, std::string>& expected,
    std::string_view ep_name,
    ILogger& logger) {
  if (expected.empty()) {
    logger.Log(LogLevel::Warning,
               fmt::format("{}: expected hash map is empty", ep_name));
    return false;
  }

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
