// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "contracts/tool_definitions.h"

#include "exception.h"
#include "inferencing/session/request.h"
#include "util/string_utils.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace fl {
namespace tools {

namespace {

/// The neutral schema for a function tool that declares no parameters. The registry requires a
/// schema; this is the JSON Schema spelling of "no constraints stated".
constexpr const char* kEmptyFunctionSchema = "{}";

constexpr const char* kTextFormatType = "text";

/// Wire spelling of each tool kind, used for the forced-choice option pair.
constexpr const char* kFunctionKindName = "function";
constexpr const char* kCustomKindName = "custom";

}  // namespace

void ValidateCustomToolFormat(const nlohmann::json& format, const std::string& tool_name) {
  if (format.is_null()) {
    return;
  }

  if (!format.is_object()) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT, "custom tool '", tool_name,
             "' format must be an object");
  }

  auto type = format.find("type");
  if (type == format.end() || !type->is_string()) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT, "custom tool '", tool_name,
             "' format must contain a string 'type'");
  }

  const auto type_name = type->get<std::string>();
  if (type_name != kTextFormatType) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT, "custom tool '", tool_name, "' declares format type '", type_name,
             "'; only 'text' is supported");
  }

  // A text format that also carries grammar members describes a constraint this runtime does not
  // apply. Reporting it is the only way the caller learns its constraint was never in effect.
  if (format.size() != 1) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT, "custom tool '", tool_name,
             "' declares a text format carrying additional members; 'text' takes no parameters");
  }
}

ToolDefinition MakeFunctionTool(std::string name, std::string description, std::string json_schema,
                                bool description_present, bool parameters_present, std::optional<bool> strict) {
  if (strict.value_or(false)) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT,
             "strict function tools are not supported");
  }

  ToolDefinition definition;
  definition.name = std::move(name);
  definition.description = std::move(description);
  definition.json_schema = json_schema.empty() ? kEmptyFunctionSchema : std::move(json_schema);
  definition.kind = ToolKind::kFunction;
  definition.include_description_in_prompt = description_present;
  definition.include_parameters_in_prompt = parameters_present;
  return definition;
}

ToolDefinition MakeCustomTool(std::string name, std::string description, bool description_present) {
  ToolDefinition definition;
  definition.name = std::move(name);
  definition.description = std::move(description);
  definition.kind = ToolKind::kCustom;
  definition.include_description_in_prompt = description_present;
  return definition;
}

std::unordered_map<std::string, ToolKind> KindsByName(const std::vector<ToolDefinition>& definitions) {
  std::unordered_map<std::string, ToolKind> kinds;
  kinds.reserve(definitions.size());

  // Every registered definition is named, so every one of them is indexable.
  for (const auto& definition : definitions) {
    kinds.emplace(definition.name, definition.kind);
  }

  return kinds;
}

void NarrowToForcedTool(std::vector<ToolDefinition>& definitions, const std::string& name, ToolKind kind) {
  auto forced = std::find_if(definitions.begin(), definitions.end(), [&](const ToolDefinition& definition) {
    return definition.name == name && definition.kind == kind;
  });

  if (forced == definitions.end()) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT,
             "tool_choice references an undeclared tool or the wrong tool kind: " + name);
  }

  ToolDefinition only = std::move(*forced);
  definitions.clear();
  definitions.push_back(std::move(only));
}

void RetainAllowedTools(std::vector<ToolDefinition>& definitions, const std::vector<std::string>& allowed_names) {
  std::unordered_set<std::string> allowed;
  allowed.reserve(allowed_names.size());
  for (const auto& name : allowed_names) {
    allowed.insert(ToLower(name));
  }

  auto excluded = std::remove_if(definitions.begin(), definitions.end(), [&](const ToolDefinition& definition) {
    return allowed.count(ToLower(definition.name)) == 0;
  });

  definitions.erase(excluded, definitions.end());
}

void RecordForcedToolChoice(Request& request, const std::string& name, ToolKind kind) {
  if (name.empty()) {
    return;
  }

  request.forced_tool_choice = ForcedToolChoice{name, kind};
}

std::optional<ForcedToolChoice> ReadForcedToolChoice(const Request& request) {
  return request.forced_tool_choice;
}

}  // namespace tools
}  // namespace fl
