// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "logger.h"
#include "telemetry/telemetry_action_tracker.h"
#include "telemetry/telemetry_logger.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace fl;

namespace {

struct LogEntry {
  LogLevel level;
  std::string message;
};

class RecordingLogger : public ILogger {
 public:
  void Log(LogLevel level, std::string_view message) override {
    entries.push_back(LogEntry{level, std::string(message)});
  }

  std::vector<LogEntry> entries;
};

struct ActionCall {
  Action action;
  ActionStatus status;
  std::string user_agent;
  bool indirect;
  int64_t duration_ms;
};

class RecordingTelemetry : public ITelemetry {
 public:
  void RecordAction(Action action, ActionStatus status,
                    const std::string& user_agent,
                    bool indirect, int64_t duration_ms) override {
    action_calls.push_back(ActionCall{action, status, user_agent, indirect, duration_ms});
  }

  void RecordException(Action action, const std::exception& exception) override {
    exception_calls.emplace_back(action, exception.what());
  }

  void RecordModelUsage(const std::string& model_id,
                        int64_t prompt_tokens,
                        int64_t completion_tokens,
                        int64_t duration_ms) override {
    model_usage_calls.push_back(
        ModelUsageCall{model_id, prompt_tokens, completion_tokens, duration_ms});
  }

  void RecordModelId(Action action, const std::string& model_id) override {
    model_id_calls.emplace_back(action, model_id);
  }

  struct ModelUsageCall {
    std::string model_id;
    int64_t prompt_tokens;
    int64_t completion_tokens;
    int64_t duration_ms;
  };

  std::vector<ActionCall> action_calls;
  std::vector<std::pair<Action, std::string>> exception_calls;
  std::vector<ModelUsageCall> model_usage_calls;
  std::vector<std::pair<Action, std::string>> model_id_calls;
};

}  // namespace

TEST(TelemetryLoggerTest, RecordActionIncludesConcreteFields) {
  RecordingLogger logger;
  TelemetryLogger telemetry("foundry-local", logger);

  telemetry.RecordAction(Action::kModelDownload, ActionStatus::kSuccess,
                         "cli/1.0", false, 1234);

  ASSERT_EQ(logger.entries.size(), 1u);
  EXPECT_EQ(logger.entries[0].level, LogLevel::Debug);
  EXPECT_NE(logger.entries[0].message.find("AppName:foundry-local"), std::string::npos);
  EXPECT_NE(logger.entries[0].message.find("UserAgent:cli/1.0"), std::string::npos);
  EXPECT_NE(logger.entries[0].message.find("Command:ModelDownload"), std::string::npos);
  EXPECT_NE(logger.entries[0].message.find("Status:Success"), std::string::npos);
  EXPECT_NE(logger.entries[0].message.find("Direct:true"), std::string::npos);
  EXPECT_NE(logger.entries[0].message.find("Time:1234ms"), std::string::npos);
}

TEST(TelemetryLoggerTest, RecordExceptionAndModelEventsIncludeSpecificValues) {
  RecordingLogger logger;
  TelemetryLogger telemetry("foundry-local", logger);

  telemetry.RecordException(Action::kModelLoad, std::runtime_error("config missing"));
  telemetry.RecordModelUsage("phi-3-mini", 17, 31, 250);
  telemetry.RecordModelId(Action::kModelLoad, "phi-3-mini");

  ASSERT_EQ(logger.entries.size(), 3u);
  EXPECT_NE(logger.entries[0].message.find("Command:ModelLoad"), std::string::npos);
  EXPECT_NE(logger.entries[0].message.find("Exception:config missing"), std::string::npos);

  EXPECT_NE(logger.entries[1].message.find("ModelUsage: model=phi-3-mini"), std::string::npos);
  EXPECT_NE(logger.entries[1].message.find("prompt_tokens=17"), std::string::npos);
  EXPECT_NE(logger.entries[1].message.find("completion_tokens=31"), std::string::npos);
  EXPECT_NE(logger.entries[1].message.find("duration=250ms"), std::string::npos);

  EXPECT_NE(logger.entries[2].message.find("Command:ModelLoad"), std::string::npos);
  EXPECT_NE(logger.entries[2].message.find("ModelId:phi-3-mini"), std::string::npos);
}

TEST(ActionTrackerTest, DestructorRecordsFailureByDefaultWithoutModelId) {
  RecordingTelemetry telemetry;

  {
    ActionTracker tracker(Action::kModelDelete, telemetry, "cli/2.0", true);
  }

  ASSERT_EQ(telemetry.action_calls.size(), 1u);
  EXPECT_EQ(telemetry.action_calls[0].action, Action::kModelDelete);
  EXPECT_EQ(telemetry.action_calls[0].status, ActionStatus::kFailure);
  EXPECT_EQ(telemetry.action_calls[0].user_agent, "cli/2.0");
  EXPECT_TRUE(telemetry.action_calls[0].indirect);
  EXPECT_GE(telemetry.action_calls[0].duration_ms, 0);
  EXPECT_TRUE(telemetry.model_id_calls.empty());
}

TEST(ActionTrackerTest, RecordsExceptionSuccessAndModelId) {
  RecordingTelemetry telemetry;

  {
    ActionTracker tracker(Action::kModelLoad, telemetry, "cli/3.0", false);
    tracker.RecordException(std::runtime_error("failed to load"));
    tracker.SetModelId("phi-3-mini");
    tracker.SetStatus(ActionStatus::kSuccess);
  }

  ASSERT_EQ(telemetry.exception_calls.size(), 1u);
  EXPECT_EQ(telemetry.exception_calls[0].first, Action::kModelLoad);
  EXPECT_EQ(telemetry.exception_calls[0].second, "failed to load");

  ASSERT_EQ(telemetry.action_calls.size(), 1u);
  EXPECT_EQ(telemetry.action_calls[0].action, Action::kModelLoad);
  EXPECT_EQ(telemetry.action_calls[0].status, ActionStatus::kSuccess);
  EXPECT_EQ(telemetry.action_calls[0].user_agent, "cli/3.0");
  EXPECT_FALSE(telemetry.action_calls[0].indirect);
  EXPECT_GE(telemetry.action_calls[0].duration_ms, 0);

  ASSERT_EQ(telemetry.model_id_calls.size(), 1u);
  EXPECT_EQ(telemetry.model_id_calls[0].first, Action::kModelLoad);
  EXPECT_EQ(telemetry.model_id_calls[0].second, "phi-3-mini");
}