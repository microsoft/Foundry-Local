// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "inferencing/generative/toolcalling/tool_call_context.h"

#include <string>

namespace fl {

/// Build a Lark grammar string for ORT GenAI's SetGuidance.
///
/// Grammar controls whether the model produces text, tool calls, chain-of-thought
/// reasoning, or combinations thereof, using special tokens when available.
///
/// Legend:
///   cot          = chain-of-thought output with newline at the end
///   THINK_TEXT   = chain-of-thought text output
///   output       = output row (text and/or tool call)
///   TEXT         = text output
///   toolcall     = tool call output (with known ids)
///   functioncall = JSON schemas for each registered tool
///
/// | Case | Description                                                                                        |
/// |------|----------------------------------------------------------------------------------------------------|
/// |  1   | Return text only                                                                                   |
/// |  2   | Return tool call only (known tool call token ids)                                                  |
/// |  3   | Return tool call only (unknown tool call token ids)                                                |
/// |  4   | Return text or tool call (known tool call token ids)                                               |
/// |  5   | Return text or tool call (unknown tool call token ids)                                             |
/// |  6   | Return chain-of-thought + text only (known think token ids)                                        |
/// |  7   | Return chain-of-thought + text only (unknown think token ids)                                      |
/// |  8   | Return chain-of-thought + tool call only (known think token ids, known tool call token ids)        |
/// |  9   | Return chain-of-thought + tool call only (unknown think token ids, known tool call token ids)      |
/// |  10  | Return chain-of-thought + tool call only (known think token ids, unknown tool call token ids)      |
/// |  11  | Return chain-of-thought + tool call only (unknown think token ids, unknown tool call token ids)    |
/// |  12  | Return chain-of-thought + text or tool call (known think token ids, known tool call token ids)     |
/// |  13  | Return chain-of-thought + text or tool call (unknown think token ids, known tool call token ids)   |
/// |  14  | Return chain-of-thought + text or tool call (known think token ids, unknown tool call token ids)   |
/// |  15  | Return chain-of-thought + text or tool call (unknown think token ids, unknown tool call token ids) |
///
/// See grammar.cc for the full grammar pattern for each case.
///
/// @param ctx           Tool calling context with flags and tokens (includes reasoning state)
/// @param json_schema   JSON schema string for the tool definitions (may be empty)
/// @return              Lark grammar string suitable for SetGuidance("lark_grammar", ...)
std::string BuildLarkGrammar(const ToolCallContext& ctx,
                             const std::string& json_schema);

/// Build a JSON schema string for tool call guidance.
///
/// Constructs a JSON schema array with anyOf entries for each tool definition.
/// The schema constrains the model's output to valid tool call JSON.
///
/// If no tools or tool_output is false, returns "{}".
///
/// @param ctx  Tool calling context (reads tools_json for tool definitions)
/// @return     JSON schema string suitable for SetGuidance("json_schema", ...)
///             or for embedding in Lark grammar as %json directive
std::string BuildToolJsonSchema(const ToolCallContext& ctx);

}  // namespace fl
