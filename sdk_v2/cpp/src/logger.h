// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "log_level.h"

#include <exception>
#include <string_view>

namespace fl {

/// Abstract logging interface. Consumers can provide their own implementation.
class ILogger {
 public:
  virtual ~ILogger() = default;
  virtual void Log(LogLevel level, std::string_view message) = 0;
  virtual void Log(const std::exception& ex, LogLevel level = LogLevel::Error) {
    Log(level, ex.what());
  }
};

/// Default logger that writes to stderr.
class StderrLogger : public ILogger {
 public:
  void Log(LogLevel level, std::string_view message) override;
};

}  // namespace fl
