// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "telemetry/telemetry_logger.h"

#include <fmt/format.h>

namespace fl {

TelemetryLogger::TelemetryLogger(const std::string& app_name, ILogger& logger)
    : app_name_(app_name), logger_(logger) {
}

void TelemetryLogger::RecordAction(Action action, ActionStatus status, const std::string& user_agent,
                                   bool indirect, int64_t duration_ms) {
  logger_.Log(LogLevel::Debug,
              fmt::format("[Telemetry] AppName:{} UserAgent:{} Command:{} Status:{} Direct:{} Time:{}ms",
                          app_name_, user_agent, ActionToString(action),
                          ActionStatusToString(status), !indirect, duration_ms));
}

void TelemetryLogger::RecordException(Action action, const std::exception& exception) {
  logger_.Log(LogLevel::Debug,
              fmt::format("[Telemetry] AppName:{} Command:{} Exception:{}",
                          app_name_, ActionToString(action), exception.what()));
}

void TelemetryLogger::RecordModelUsage(const std::string& model_id,
                                       int64_t prompt_tokens,
                                       int64_t completion_tokens,
                                       int64_t duration_ms) {
  logger_.Log(LogLevel::Debug,
              fmt::format("[Telemetry] AppName:{} ModelUsage: model={} prompt_tokens={} "
                          "completion_tokens={} duration={}ms",
                          app_name_, model_id, prompt_tokens, completion_tokens, duration_ms));
}

void TelemetryLogger::RecordModelId(Action action, const std::string& model_id) {
  logger_.Log(LogLevel::Debug,
              fmt::format("[Telemetry] AppName:{} Command:{} ModelId:{}",
                          app_name_, ActionToString(action), model_id));
}

}  // namespace fl
