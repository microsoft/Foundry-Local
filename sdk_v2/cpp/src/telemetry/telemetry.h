// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <cstdint>
#include <exception>
#include <string>
#include <string_view>

namespace fl {

/// Telemetry action identifiers matching the C# ITelemetry.Action enum.
// TODO: This is a lie. The enum values don't match. Do they need to?
enum class Action {
  kInvalid = 0,

  // Core lifecycle
  kCoreInitialize = 1,
  kCoreServiceStart = 2,
  kCoreServiceStop = 3,

  // Session (inference)
  kSessionCreate = 50,
  kSessionProcessRequest = 51,

  // Model management
  kModelLoad = 100,
  kModelUnload = 101,
  kModelDownload = 102,
  kModelDelete = 103,
  kModelList = 104,

  // OpenAI Chat/Audio/Embeddings APIs
  kOpenAIChatCompletions = 200,
  kOpenAIModelList = 201,
  kOpenAIModelRetrieve = 202,
  kOpenAIAudioTranscribe = 203,
  kOpenAIEmbeddings = 204,

  // OpenAI Responses API
  kOpenAIResponsesCreate = 300,
  kOpenAIResponsesGet = 301,
  kOpenAIResponsesList = 302,
  kOpenAIResponsesDelete = 303,
  kOpenAIResponsesGetInputItems = 304,

  // Audio
  kCoreAudioTranscribe = 400,
};

/// Status of a tracked telemetry action.
enum class ActionStatus {
  kFailure = 0,
  kSuccess,
  kInvalid,
  kSkipped,
};

/// Convert Action to human-readable string.
std::string_view ActionToString(Action action);

/// Convert ActionStatus to human-readable string.
std::string_view ActionStatusToString(ActionStatus status);

/// Abstract telemetry interface.
/// Implementations may send events to a telemetry backend (ETW, OpenTelemetry, etc.)
/// or simply log them. The stub TelemetryLogger logs via ILogger.
class ITelemetry {
 public:
  virtual ~ITelemetry() = default;

  /// Record a completed action with timing and status.
  virtual void RecordAction(Action action, ActionStatus status,
                            const std::string& user_agent,
                            bool indirect, int64_t duration_ms) = 0;

  /// Record an exception associated with an action.
  virtual void RecordException(Action action, const std::exception& exception) = 0;

  /// Record model usage metrics after inference.
  virtual void RecordModelUsage(const std::string& model_id,
                                int64_t prompt_tokens,
                                int64_t completion_tokens,
                                int64_t duration_ms) = 0;

  /// Record which model was used for an action.
  virtual void RecordModelId(Action action, const std::string& model_id) = 0;
};

}  // namespace fl
