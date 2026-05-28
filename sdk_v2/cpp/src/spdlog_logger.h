// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "logger.h"

#include <memory>
#include <string>

namespace spdlog {
class logger;
namespace details {
class thread_pool;
}
}

namespace fl {

/// spdlog-based logger with async logging, colored stderr, and optional daily rotating file output.
class SpdlogLogger : public ILogger {
 public:
  /// Create a logger with the given minimum severity level.
  /// If logs_dir is non-empty, a daily rotating file sink is added at <logs_dir>/foundry_local.log.
  SpdlogLogger(LogLevel min_level, const std::string& logs_dir = "");

  ~SpdlogLogger() override;

  void Log(LogLevel level, std::string_view message) override;

 private:
  std::shared_ptr<spdlog::logger> logger_;
  std::shared_ptr<spdlog::details::thread_pool> thread_pool_;
};

}  // namespace fl
