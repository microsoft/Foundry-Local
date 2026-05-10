// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
// Shared test helpers for internal API tests.
#pragma once

#include "ep_detection/ep_detector.h"

#include <map>
#include <string>
#include <vector>

namespace fl::test {

/// EP detector that only reports CPU — used by tests that load real models
/// without requiring GPU hardware.
class CpuOnlyEpDetector : public IEpDetector {
 public:
  const std::map<std::string, std::vector<std::string>>& GetAvailableDevicesToEPs() const override {
    static const std::map<std::string, std::vector<std::string>> cpu_only = {
        {"CPU", {"CPUExecutionProvider"}}};
    return cpu_only;
  }
};

}  // namespace fl::test
