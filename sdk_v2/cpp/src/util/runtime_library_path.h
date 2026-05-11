// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <string>

namespace fl {

/// Store the directory path for ORT runtime DLLs. Must be called before any ORT API usage.
/// Returns false if ORT DLLs have already been loaded (path cannot be changed after that).
bool SetRuntimeLibraryPath(const char* path);

/// Get the stored runtime library path. Returns empty string if not set.
const std::string& GetRuntimeLibraryPath();

/// Mark that ORT DLLs have been loaded. Called by the delay-load hook on Windows.
/// After this, SetRuntimeLibraryPath returns false.
void MarkOrtLoaded();

/// Reset all state. For testing only — not safe to call while ORT is in use.
void ResetRuntimeLibraryPathForTesting();

}  // namespace fl
