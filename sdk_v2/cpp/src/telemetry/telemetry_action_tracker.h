// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "telemetry/telemetry.h"

#include <chrono>
#include <string>

namespace fl {

/// RAII action tracker that records timing and status via ITelemetry on destruction.
/// Default status is kFailure — call SetStatus(kSuccess) on the happy path.
/// Matches the C# ActionTracker (IDisposable) pattern.
class ActionTracker {
 public:
  ActionTracker(Action action, ITelemetry& telemetry,
                const std::string& user_agent = "",
                bool indirect = false);
  ~ActionTracker();

  // Non-copyable, non-movable
  ActionTracker(const ActionTracker&) = delete;
  ActionTracker& operator=(const ActionTracker&) = delete;

  /// Set the final status. Default is kFailure if not called.
  void SetStatus(ActionStatus status);

  /// Record an exception associated with this action.
  void RecordException(const std::exception& exception);

  /// Associate a model ID with this action.
  void SetModelId(const std::string& model_id);

 private:
  Action action_;
  ITelemetry& telemetry_;
  std::string user_agent_;
  bool indirect_;
  ActionStatus status_ = ActionStatus::kFailure;
  std::string model_id_;
  std::chrono::steady_clock::time_point start_;
};

}  // namespace fl
