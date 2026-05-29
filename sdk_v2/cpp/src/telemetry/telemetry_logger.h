// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "telemetry/telemetry.h"
#include "logger.h"

#include <string>

namespace fl {

/// Stub ITelemetry implementation that logs telemetry events via ILogger.
/// Used as a fallback when no platform-specific telemetry backend is available.
class TelemetryLogger : public ITelemetry {
 public:
  TelemetryLogger(const std::string& app_name, ILogger& logger);

  void RecordAction(Action action, ActionStatus status, const std::string& user_agent,
                    bool indirect, int64_t duration_ms) override;

  void RecordException(Action action, const std::exception& exception) override;

  void RecordModelUsage(const std::string& model_id,
                        int64_t prompt_tokens,
                        int64_t completion_tokens,
                        int64_t duration_ms) override;

  void RecordModelId(Action action, const std::string& model_id) override;

 private:
  std::string app_name_;
  ILogger& logger_;
};

}  // namespace fl
