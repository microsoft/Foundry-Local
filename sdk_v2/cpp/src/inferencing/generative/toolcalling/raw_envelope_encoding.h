// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace fl {

/// Key under which a turn declares how one of its custom tools writes its call into visible output.
///
/// Read from `Request::options` first and from the model's string properties second, so a caller can override what
/// a model was published with and a model can carry the dialect it was trained on. The value is a JSON object — one
/// descriptor per turn — not a bag of loose scalar keys: a dialect is only meaningful as a complete (tool, start,
/// end) triple, and splitting it across three keys would let a half-configured turn parse.
inline constexpr const char* kToolOutputEncodingKey = "tool_output_encoding";

/// The only encoding kind this runtime understands. Carried explicitly so a future dialect is a new `type` value
/// that old builds reject loudly rather than silently misread as this one.
inline constexpr const char* kRawEnvelopeEncodingType = "raw_envelope";

/// How a model writes a call to one named custom tool directly into its visible output, instead of wrapping it in
/// the structured tool-call markers the rest of the tool path expects.
///
/// Some models are trained to emit a bare, line-anchored envelope — `<start_marker>\n` … `\n<end_marker>` — whose
/// body is the tool's entire text payload. Nothing inside the envelope names the tool, so the descriptor is what
/// gives those bytes a meaning: without one the same bytes are prose, and that is exactly how they are reported.
///
/// The descriptor is data, not policy. It says nothing about *when* an envelope is honoured — see
/// ToolCallContext::ActiveRawEnvelope for the forced-tool gate that decides that.
struct RawEnvelopeEncoding {
  /// Name of the custom tool an envelope resolves to. Matched exactly, and only against a custom tool.
  std::string tool_name;

  /// The line that opens an envelope, without its terminator.
  std::string start_marker;

  /// The line that closes an envelope, without its terminator.
  std::string end_marker;

  /// Value equality. Two turns share a dialect only when all three fields agree, which is also what decides whether
  /// a warm session can be reused for a following turn (see ToolCallContext::HasSameTools).
  bool operator==(const RawEnvelopeEncoding& other) const = default;
};

/// Serialize the descriptor in the same shape `from_json` accepts, so a stored one round-trips exactly.
void to_json(nlohmann::json& j, const RawEnvelopeEncoding& encoding);

/// Read and validate a descriptor.
///
/// Every rule is enforced here rather than at call sites, so a descriptor that exists is always usable:
///   - the value is an object carrying `type` == "raw_envelope";
///   - `tool_name`, `start_marker` and `end_marker` are all present, all strings, and none empty;
///   - the two markers differ — identical markers describe an envelope that closes where it opens;
///   - neither marker contains CR or LF, because both are matched as whole lines and an embedded newline would
///     describe a marker that can never occur;
///   - no other member is present. An unknown field is refused rather than ignored: it is either a typo for one of
///     the four, or a constraint this build does not apply, and silently dropping either produces a turn that reads
///     its output differently than the caller asked for.
///
/// @throws fl::Exception FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT for any of the above.
void from_json(const nlohmann::json& j, RawEnvelopeEncoding& encoding);

/// Parse a descriptor from the raw string carried in request options or a model property.
///
/// An empty or whitespace-only string means "not configured" and yields nullopt — that is how a request says "leave
/// the model's own value alone". Anything else must be a valid descriptor.
///
/// @param text    The raw configured value.
/// @param source  Where it came from, used verbatim in error messages (e.g. "request option" / "model property").
/// @throws fl::Exception FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT when `text` is non-blank but is not valid JSON or not
///         a valid descriptor. Never falls back to another source: a caller that configured a dialect and spelled it
///         wrong must be told, not quietly given a different one.
std::optional<RawEnvelopeEncoding> ParseToolOutputEncoding(const std::string& text, std::string_view source);

}  // namespace fl
