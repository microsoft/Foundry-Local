// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
// Shared test helpers for internal API tests.
#pragma once

#include "ep_detection/ep_detector.h"
#include "logger.h"

#include <map>
#include <string>
#include <vector>

namespace fl::test {

/// EP detector that only reports CPU — used by tests that load real models
/// without requiring GPU hardware.
class CpuOnlyEpDetector : public IEpDetector {
 public:
  std::map<std::string, std::vector<std::string>> GetAvailableDevicesToEPs() const override {
    return {{"CPU", {"CPUExecutionProvider"}}};
  }
};

/// Sink logger that drops every message. Test-only — production code that needs an `ILogger&`
/// must be wired to a real logger, not silenced.
class NullLogger : public ILogger {
 public:
  void Log(LogLevel /*level*/, std::string_view /*message*/) override {}
};

/// Returns a process-wide `NullLogger` for tests that need to satisfy an `ILogger&` parameter
/// but don't care about log output.
inline ILogger& NullLog() {
  static NullLogger instance;
  return instance;
}

}  // namespace fl::test
