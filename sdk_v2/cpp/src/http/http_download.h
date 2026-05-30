// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <string>

namespace fl {

class ILogger;

/// Download a file from an HTTP(S) URL to a local path.
/// Supports progress reporting and cancellation.
/// @param url  The URL to download from.
/// @param destination  Local file path to write to.
/// @param user_agent  HTTP User-Agent header.
/// @param cancel_flag  Set to true to cancel. nullptr if not needed.
/// @param progress_cb  Called with percent 0.0-100.0. Empty = no callback.
/// @param logger  Logger for diagnostic output on failure.
/// @return true on success, false on failure.
bool HttpDownloadFile(const std::string& url,
                      const std::filesystem::path& destination,
                      const std::string& user_agent,
                      std::atomic<bool>* cancel_flag,
                      std::function<void(float percent)> progress_cb,
                      ILogger& logger);

}  // namespace fl
