// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace fl {

/// Information about a discoverable execution provider.
struct EpInfo {
  std::string name;
  bool is_registered = false;
};

/// Result of a bulk EP download-and-register operation.
struct EpDownloadResult {
  bool success = false;
  bool cancelled = false;
  std::string status;
  std::vector<std::string> registered_eps;
  std::vector<std::string> failed_eps;
};

/// Callback to register an EP library with the ORT environment.
/// @param registration_name  Unique name for the EP (e.g., "DML", "QNN").
/// @param library_path  Full filesystem path to the EP DLL/SO.
/// @return true if registration succeeded.
using EpRegistrationCallback = std::function<bool(const std::string& registration_name,
                                                  const std::filesystem::path& library_path)>;

}  // namespace fl
