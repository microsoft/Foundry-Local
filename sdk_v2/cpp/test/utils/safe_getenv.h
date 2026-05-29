// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
// Platform-safe getenv wrapper shared across all test targets.
#pragma once

#include <cstdlib>
#include <string>

#ifdef _WIN32
#include <cstring>  // for free()
#endif

namespace fl::test {

/// Platform-safe getenv wrapper. Returns empty string if the variable is not set.
/// Uses _dupenv_s on Windows to avoid MSVC C4996 warning-as-error on std::getenv.
inline std::string SafeGetEnv(const char* name) {
#ifdef _WIN32
  char* value = nullptr;
  size_t len = 0;
  if (_dupenv_s(&value, &len, name) == 0 && value != nullptr) {
    std::string result(value);
    free(value);
    return result;
  }

  return "";
#else
  const char* value = std::getenv(name);
  return value ? std::string(value) : "";
#endif
}

}  // namespace fl::test
