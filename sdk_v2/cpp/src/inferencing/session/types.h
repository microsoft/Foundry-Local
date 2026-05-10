// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <string>

namespace fl {

/// The kind of inference a session performs.
enum class SessionType {
  kChat,
  kAudio,
  kPredictive,
  kEmbeddings,
};

struct ToolDefinition {
  std::string name;
  std::string description;
  std::string json_schema;
};

}  // namespace fl
