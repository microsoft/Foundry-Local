// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "inferencing/generative/toolcalling/tool_call_utils.h"
#include "items/tool_call_item.h"

#include <nlohmann/json.hpp>

#include <random>

namespace fl {

namespace {

/// Generate a random alphanumeric string of the given length.
std::string RandomAlphanumeric(int length) {
  static constexpr char kChars[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  static thread_local std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<int> dist(0, sizeof(kChars) - 2);

  std::string result;
  result.reserve(length);

  for (int i = 0; i < length; ++i) {
    result += kChars[dist(rng)];
  }

  return result;
}

/// Try to parse a JSON string as a list of tool calls.
/// Handles both array and single-object formats:
///   [{"name": "fn", "arguments": {...}}]
///   {"name": "fn", "arguments": {...}}
std::vector<ParsedToolCall> DeserializeToolCalls(const std::string& json_text) {
  std::vector<ParsedToolCall> results;

  try {
    auto json = nlohmann::json::parse(json_text);

    auto parse_one = [&](const nlohmann::json& call) {
      if (!call.is_object() || !call.contains("name")) {
        return;
      }

      ParsedToolCall tc;
      tc.id = GenerateToolCallId();
      tc.name = call["name"].get<std::string>();

      // Arguments can be under "arguments" or "parameters"
      if (call.contains("arguments")) {
        if (call["arguments"].is_string()) {
          tc.arguments = call["arguments"].get<std::string>();
        } else {
          tc.arguments = call["arguments"].dump();
        }
      } else if (call.contains("parameters")) {
        if (call["parameters"].is_string()) {
          tc.arguments = call["parameters"].get<std::string>();
        } else {
          tc.arguments = call["parameters"].dump();
        }
      }

      results.push_back(std::move(tc));
    };

    if (json.is_array()) {
      for (const auto& item : json) {
        parse_one(item);
      }
    } else if (json.is_object()) {
      parse_one(json);
    }
  } catch (const nlohmann::json::parse_error&) {
    // Invalid JSON — return whatever we have so far (may be empty)
  }

  return results;
}

}  // namespace

std::string GenerateToolCallId() {
  return "call_" + RandomAlphanumeric(9);
}

std::vector<ParsedToolCall> ParseToolCalls(const std::string& text,
                                           const std::string& tool_call_start,
                                           const std::string& tool_call_end) {
  std::vector<ParsedToolCall> all_calls;

  if (tool_call_start.empty() || tool_call_end.empty()) {
    return all_calls;
  }

  // Find all occurrences of tool_call_start ... tool_call_end in the text
  size_t search_pos = 0;

  while (search_pos < text.size()) {
    size_t start_pos = text.find(tool_call_start, search_pos);
    if (start_pos == std::string::npos) {
      break;
    }

    size_t content_start = start_pos + tool_call_start.size();
    size_t end_pos = text.find(tool_call_end, content_start);
    if (end_pos == std::string::npos) {
      break;
    }

    std::string content = text.substr(content_start, end_pos - content_start);
    auto calls = DeserializeToolCalls(content);

    for (auto& call : calls) {
      all_calls.push_back(std::move(call));
    }

    search_pos = end_pos + tool_call_end.size();
  }

  return all_calls;
}

std::vector<std::unique_ptr<Item>> ToolCallsToItems(const std::vector<ParsedToolCall>& calls) {
  std::vector<std::unique_ptr<Item>> items;
  items.reserve(calls.size());

  for (const auto& call : calls) {
    items.push_back(std::make_unique<ToolCallItem>(call.id, call.name, call.arguments));
  }

  return items;
}

}  // namespace fl
