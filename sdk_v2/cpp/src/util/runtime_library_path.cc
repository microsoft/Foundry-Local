// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "util/runtime_library_path.h"

#include <mutex>
#include <string>

namespace fl {

namespace {

std::mutex g_path_mutex;
std::string g_runtime_library_path;
bool g_ort_loaded = false;

}  // anonymous namespace

bool SetRuntimeLibraryPath(const char* path) {
  std::lock_guard<std::mutex> lock(g_path_mutex);

  if (g_ort_loaded) {
    return false;
  }

  g_runtime_library_path = path ? path : "";
  return true;
}

const std::string& GetRuntimeLibraryPath() {
  // No lock needed — reads only happen after write (before any ORT API call),
  // and SetRuntimeLibraryPath is the only writer. The happens-before relationship
  // is established by the caller's sequencing (set path, then use SDK).
  return g_runtime_library_path;
}

void MarkOrtLoaded() {
  std::lock_guard<std::mutex> lock(g_path_mutex);
  g_ort_loaded = true;
}

void ResetRuntimeLibraryPathForTesting() {
  std::lock_guard<std::mutex> lock(g_path_mutex);
  g_runtime_library_path.clear();
  g_ort_loaded = false;
}

}  // namespace fl
