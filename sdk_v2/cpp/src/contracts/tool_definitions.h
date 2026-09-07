// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "inferencing/session/types.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace fl {

struct Request;

namespace tools {

// ========================================================================
// Wire tool declarations → core tool definitions.
//
// Chat Completions and Responses spell a tool differently — Chat nests the declaration under
// "function" / "custom", Responses inlines it next to "type" — but both describe the same two
// kinds of tool, and the runtime has exactly one representation of them: fl::ToolDefinition,
// carrying a name, a description, a schema and a ToolKind. Every HTTP surface converts to that
// representation here and registers it on the session; the session's registry is the single place
// that serializes tools for the prompt and the single authority on what kind each name is.
//
// Nothing downstream of these helpers knows which surface a tool arrived on, and no surface hands
// the session a pre-serialized tools array.
// ========================================================================

/// Reject any custom-tool `format` this runtime cannot honour.
///
/// A custom tool takes free-form text: the model is prompted with the synthesized single-string
/// schema and whatever it produces comes back verbatim. `text` is the only format that describes
/// what actually happens, so it is the only one accepted. A grammar-constrained format is refused
/// rather than quietly downgraded — a caller that asked for constrained output would otherwise
/// receive unconstrained output with no indication that its constraint was dropped.
///
/// Accepted: a null value (absent format) and an object `{"type":"text"}` carrying nothing else.
///
/// @throws fl::Exception (FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT) for a non-object format, a missing
///         or non-string `type`, a `grammar` type, an unknown type, and a text format carrying
///         additional members (which would describe a constraint that is not applied).
void ValidateCustomToolFormat(const nlohmann::json& format, const std::string& tool_name);

/// Core definition for a function tool declared on the wire.
///
/// An empty `json_schema` becomes `{}`: the registry requires every function tool to carry a
/// schema, and `{}` is how "declares no parameters" is spelled in JSON Schema. A schema the caller
/// did supply is passed through unchanged, so what the model is prompted with stays JSON-equivalent
/// to what was requested.
ToolDefinition MakeFunctionTool(std::string name, std::string description, std::string json_schema,
                                bool description_present = true, bool parameters_present = true,
                                std::optional<bool> strict = std::nullopt);

/// Core definition for a text custom tool declared on the wire.
///
/// The schema is left empty on purpose: the registry synthesizes the single required string `input`
/// schema, and a caller-supplied schema is rejected. This is what keeps raw custom input out of
/// function-argument handling — a custom tool never carries a function schema anywhere.
ToolDefinition MakeCustomTool(std::string name, std::string description, bool description_present = true);

/// Name-to-kind index over one immutable snapshot of tool definitions.
///
/// A turn takes the snapshot once and derives this index from it, so the kinds used to normalize a replayed call and
/// to read a produced one are the same kinds that shaped the prompt — the registry stays mutable from other threads,
/// and re-reading it mid-turn could resolve a turn's own output against a tool set that never prompted it.
std::unordered_map<std::string, ToolKind> KindsByName(const std::vector<ToolDefinition>& definitions);

/// Narrow a declared tool set to the single tool a forced `tool_choice` names, matching on both
/// name and kind so a function and a custom tool sharing a name can never be swapped for each
/// other.
///
/// A forced tool that was not declared with the requested kind is rejected.
void NarrowToForcedTool(std::vector<ToolDefinition>& definitions, const std::string& name, ToolKind kind);

/// Keep only the definitions whose name appears in `allowed_names`, compared case-insensitively to
/// match the reference implementation.
///
/// Order is preserved, and `allowed_names` is a filter rather than a selection: repeated entries
/// keep a tool once, and entries naming a tool that was never declared match nothing.
void RetainAllowedTools(std::vector<ToolDefinition>& definitions, const std::vector<std::string>& allowed_names);

/// Record an explicit forced choice on a trusted provider-converted session request.
///
/// Call this *before* NarrowToForcedTool, from every surface that resolves a forced `tool_choice`, so what reaches
/// the session is the caller's instruction and not an inference drawn from a tool set that filtering has already
/// reduced to one entry.
void RecordForcedToolChoice(Request& request, const std::string& name, ToolKind kind);

/// Read back a forced choice recorded by RecordForcedToolChoice.
///
/// Returns nullopt when no tool was forced, when the name is empty, or when the kind is missing or unrecognized —
/// all of which mean "the caller did not name a tool", which is the safe reading: behaviour gated on an explicit
/// choice stays off rather than switching on for a malformed one.
std::optional<ForcedToolChoice> ReadForcedToolChoice(const Request& request);

}  // namespace tools
}  // namespace fl
