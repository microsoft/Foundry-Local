// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "inferencing/generative/toolcalling/tool_call_utils.h"
#include "items/tool_call_item.h"

#include <nlohmann/json.hpp>

#include <cctype>
#include <optional>
#include <random>
#include <string_view>

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

struct ToolCallSource {
  std::optional<std::string_view> arguments;
  std::optional<std::string_view> parameters;
};

/// Structural scanner used after nlohmann has validated the JSON. It records source spans rather
/// than searching for member text, so escaped strings and nested objects cannot create false hits.
class JsonSourceScanner {
 public:
  explicit JsonSourceScanner(std::string_view source) : source_(source) {}

  std::optional<std::vector<ToolCallSource>> ScanToolCalls() {
    SkipWhitespace();
    std::vector<ToolCallSource> calls;

    if (Peek() == '[') {
      ++position_;
      SkipWhitespace();
      if (Peek() == ']') {
        ++position_;
      } else {
        while (true) {
          auto call = ScanToolCallObject();
          if (!call.has_value()) {
            return std::nullopt;
          }

          calls.push_back(*call);
          SkipWhitespace();
          if (Peek() == ']') {
            ++position_;
            break;
          }

          if (Peek() != ',') {
            return std::nullopt;
          }

          ++position_;
          SkipWhitespace();
        }
      }
    } else {
      auto call = ScanToolCallObject();
      if (!call.has_value()) {
        return std::nullopt;
      }

      calls.push_back(*call);
    }

    SkipWhitespace();
    return position_ == source_.size() ? std::optional{std::move(calls)} : std::nullopt;
  }

 private:
  char Peek() const {
    return position_ < source_.size() ? source_[position_] : '\0';
  }

  void SkipWhitespace() {
    while (position_ < source_.size() &&
           std::isspace(static_cast<unsigned char>(source_[position_]))) {
      ++position_;
    }
  }

  bool SkipString() {
    if (Peek() != '"') {
      return false;
    }

    ++position_;
    while (position_ < source_.size()) {
      const char current = source_[position_++];
      if (current == '"') {
        return true;
      }

      if (current == '\\') {
        if (position_ >= source_.size()) {
          return false;
        }

        ++position_;
      }
    }

    return false;
  }

  bool SkipValue() {
    SkipWhitespace();
    if (Peek() == '"') {
      return SkipString();
    }

    if (Peek() == '{') {
      ++position_;
      SkipWhitespace();
      if (Peek() == '}') {
        ++position_;
        return true;
      }

      while (true) {
        if (!SkipString()) {
          return false;
        }

        SkipWhitespace();
        if (Peek() != ':') {
          return false;
        }

        ++position_;
        if (!SkipValue()) {
          return false;
        }

        SkipWhitespace();
        if (Peek() == '}') {
          ++position_;
          return true;
        }

        if (Peek() != ',') {
          return false;
        }

        ++position_;
        SkipWhitespace();
      }
    }

    if (Peek() == '[') {
      ++position_;
      SkipWhitespace();
      if (Peek() == ']') {
        ++position_;
        return true;
      }

      while (true) {
        if (!SkipValue()) {
          return false;
        }

        SkipWhitespace();
        if (Peek() == ']') {
          ++position_;
          return true;
        }

        if (Peek() != ',') {
          return false;
        }

        ++position_;
      }
    }

    const size_t start = position_;
    while (position_ < source_.size()) {
      const char current = source_[position_];
      if (current == ',' || current == ']' || current == '}' ||
          std::isspace(static_cast<unsigned char>(current))) {
        break;
      }

      ++position_;
    }

    return position_ > start;
  }

  std::optional<ToolCallSource> ScanToolCallObject() {
    SkipWhitespace();
    if (Peek() != '{') {
      return std::nullopt;
    }

    ++position_;
    SkipWhitespace();
    ToolCallSource result;
    if (Peek() == '}') {
      ++position_;
      return result;
    }

    while (true) {
      const size_t key_start = position_;
      if (!SkipString()) {
        return std::nullopt;
      }

      const auto key = nlohmann::json::parse(source_.substr(key_start, position_ - key_start))
                           .get<std::string>();
      SkipWhitespace();
      if (Peek() != ':') {
        return std::nullopt;
      }

      ++position_;
      SkipWhitespace();
      const size_t value_start = position_;
      if (!SkipValue()) {
        return std::nullopt;
      }

      const auto value = source_.substr(value_start, position_ - value_start);
      if (key == "arguments") {
        result.arguments = value;
      } else if (key == "parameters") {
        result.parameters = value;
      }

      SkipWhitespace();
      if (Peek() == '}') {
        ++position_;
        return result;
      }

      if (Peek() != ',') {
        return std::nullopt;
      }

      ++position_;
      SkipWhitespace();
    }
  }

  std::string_view source_;
  size_t position_ = 0;
};

std::optional<ParsedToolCall> ParseOneToolCall(const nlohmann::json& call,
                                               const ToolCallSource& source) {
  if (!call.is_object()) {
    return std::nullopt;
  }

  auto name_it = call.find("name");
  if (name_it == call.end() || !name_it->is_string() || name_it->get<std::string>().empty()) {
    return std::nullopt;
  }

  ParsedToolCall tc;
  tc.name = name_it->get<std::string>();

  if (auto args_it = call.find("arguments"); args_it != call.end()) {
    if (!source.arguments.has_value()) {
      return std::nullopt;
    }

    tc.parsed_arguments = *args_it;
    tc.argument_source = std::string(*source.arguments);
    tc.arguments = args_it->is_string() ? args_it->get<std::string>() : args_it->dump();
  } else if (auto params_it = call.find("parameters"); params_it != call.end()) {
    if (!source.parameters.has_value()) {
      return std::nullopt;
    }

    tc.parsed_arguments = *params_it;
    tc.argument_source = std::string(*source.parameters);
    tc.arguments = params_it->is_string() ? params_it->get<std::string>() : params_it->dump();
  }

  return tc;
}

/// Try to parse a JSON string as a list of tool calls.
/// Handles both array and single-object formats:
///   [{"name": "fn", "arguments": {...}}]
///   {"name": "fn", "arguments": {...}}
std::vector<ParsedToolCall> DeserializeToolCalls(const std::string& json_text) {
  try {
    auto json = nlohmann::json::parse(json_text);
    auto sources = JsonSourceScanner(json_text).ScanToolCalls();
    if (!sources.has_value()) {
      return {};
    }

    std::vector<ParsedToolCall> results;

    if (json.is_array()) {
      if (sources->size() != json.size()) {
        return {};
      }

      results.reserve(json.size());

      for (size_t index = 0; index < json.size(); ++index) {
        auto parsed = ParseOneToolCall(json[index], (*sources)[index]);
        if (!parsed) {
          return {};
        }

        results.push_back(std::move(*parsed));
      }
    } else if (json.is_object()) {
      if (sources->size() != 1) {
        return {};
      }

      auto parsed = ParseOneToolCall(json, sources->front());
      if (!parsed) {
        return {};
      }

      results.push_back(std::move(*parsed));
    }

    for (auto& result : results) {
      result.id = GenerateToolCallId();
    }

    return results;
  } catch (const nlohmann::json::exception&) {
    return {};
  }
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
