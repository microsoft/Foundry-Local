// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "logger.h"

#include <iostream>

namespace fl {

void StderrLogger::Log(LogLevel level, std::string_view message) {
  const char* level_str = "UNKNOWN";
  switch (level) {
    case LogLevel::Verbose:
      level_str = "VERBOSE";
      break;
    case LogLevel::Debug:
      level_str = "DEBUG";
      break;
    case LogLevel::Information:
      level_str = "INFO";
      break;
    case LogLevel::Warning:
      level_str = "WARN";
      break;
    case LogLevel::Error:
      level_str = "ERROR";
      break;
    case LogLevel::Fatal:
      level_str = "FATAL";
      break;
  }

  std::cerr << "[" << level_str << "] " << message << "\n";
}

}  // namespace fl
