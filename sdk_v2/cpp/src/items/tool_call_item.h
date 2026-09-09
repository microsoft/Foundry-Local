// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "inferencing/session/types.h"
#include "items/item.h"

#include <optional>
#include <string>

namespace fl {

/// Model-generated request to invoke a tool.
struct ToolCallItem : Item {
  std::string call_id;
  std::string name;
  std::string arguments;
  std::optional<ToolKind> declared_kind;

  ToolCallItem(std::string call_id_in = {}, std::string name_in = {}, std::string arguments_in = {},
               std::optional<ToolKind> declared_kind_in = std::nullopt)
      : Item(FOUNDRY_LOCAL_ITEM_TOOL_CALL),
        call_id(std::move(call_id_in)),
        name(std::move(name_in)),
        arguments(std::move(arguments_in)),
        declared_kind(declared_kind_in) {}

  void SetToolCallData(const flToolCallData& new_data) {
    call_id = new_data.call_id ? new_data.call_id : "";
    name = new_data.name ? new_data.name : "";
    arguments = new_data.arguments ? new_data.arguments : "";
    declared_kind.reset();
  }

  void GetApiData(flToolCallData& out) const {
    out.version = FOUNDRY_LOCAL_API_VERSION;
    out.call_id = call_id.c_str();
    out.name = name.c_str();
    out.arguments = arguments.c_str();
  }
};

}  // namespace fl
