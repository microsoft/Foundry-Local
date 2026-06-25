// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "ep_detection/ep_utils.h"

#include "logger.h"
#include "util/sha256.h"

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace fl {

bool VerifyEpPackage(
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

void PrependDirToProcessPath([[maybe_unused]] const std::filesystem::path& dir) {
#ifdef _WIN32
  DWORD len = GetEnvironmentVariableW(L"PATH", nullptr, 0);
  std::wstring prev_path;
  if (len > 0) {
    prev_path.resize(len);
    GetEnvironmentVariableW(L"PATH", prev_path.data(), len);
    prev_path.resize(len - 1);  // remove trailing null
  }

  std::wstring new_path = dir.wstring() + L";" + prev_path;
  SetEnvironmentVariableW(L"PATH", new_path.c_str());
#endif
}

}  // namespace fl
