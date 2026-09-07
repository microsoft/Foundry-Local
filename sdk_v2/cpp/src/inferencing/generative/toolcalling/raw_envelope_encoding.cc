// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "inferencing/generative/toolcalling/raw_envelope_encoding.h"

#include "exception.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace fl {

namespace {

constexpr const char* kTypeKey = "type";
constexpr const char* kToolNameKey = "tool_name";
constexpr const char* kStartMarkerKey = "start_marker";
constexpr const char* kEndMarkerKey = "end_marker";

/// Every member a descriptor may carry. Anything else is refused — see ValidateInto.
constexpr std::array<const char*, 4> kKnownKeys{kTypeKey, kToolNameKey, kStartMarkerKey, kEndMarkerKey};

/// Prefix that names where a rejected descriptor came from. `fl::Exception::what()` already carries a code location
/// and a stack trace, so a failure is described once, where it is detected, rather than caught and re-wrapped.
std::string Describe(std::string_view source) {
  if (source.empty()) {
    return std::string(kToolOutputEncodingKey);
  }

  return std::string(source) + " " + std::string(kToolOutputEncodingKey);
}

/// Read a required non-empty string member.
std::string RequiredNonEmptyString(const nlohmann::json& j, const char* key, std::string_view source) {
  const auto it = j.find(key);

  if (it == j.end() || !it->is_string()) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT, Describe(source), " requires a string '", key, "'");
  }

  auto value = it->get<std::string>();

  if (value.empty()) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT, Describe(source), " '", key, "' must not be empty");
  }

  return value;
}

/// Markers are matched as complete lines, so one carrying its own line break could never be found.
void RejectEmbeddedNewlines(const std::string& marker, const char* key, std::string_view source) {
  if (marker.find('\n') != std::string::npos || marker.find('\r') != std::string::npos) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT, Describe(source), " '", key,
             "' must not contain a carriage return or line feed; markers are matched as whole lines");
  }
}

/// The one validation path. `from_json` and ParseToolOutputEncoding differ only in how they name the source.
void ValidateInto(const nlohmann::json& j, RawEnvelopeEncoding& encoding, std::string_view source) {
  if (!j.is_object()) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT, Describe(source), " must be a JSON object");
  }

  const auto type = j.find(kTypeKey);

  if (type == j.end() || !type->is_string()) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT, Describe(source), " requires a string 'type'");
  }

  const auto type_name = type->get<std::string>();

  if (type_name != kRawEnvelopeEncodingType) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT, Describe(source), " declares type '", type_name, "'; only '",
             kRawEnvelopeEncodingType, "' is supported");
  }

  // An unrecognized member is either a misspelling of a required one or a constraint this build cannot apply.
  // Either way the caller would get behavior it did not ask for, so it is reported instead of dropped.
  for (const auto& entry : j.items()) {
    const auto& key = entry.key();
    const bool known = std::any_of(kKnownKeys.begin(), kKnownKeys.end(),
                                   [&key](const char* candidate) { return key == candidate; });

    if (!known) {
      FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT, Describe(source), " carries unknown member '", key, "'");
    }
  }

  RawEnvelopeEncoding parsed;
  parsed.tool_name = RequiredNonEmptyString(j, kToolNameKey, source);
  parsed.start_marker = RequiredNonEmptyString(j, kStartMarkerKey, source);
  parsed.end_marker = RequiredNonEmptyString(j, kEndMarkerKey, source);

  RejectEmbeddedNewlines(parsed.start_marker, kStartMarkerKey, source);
  RejectEmbeddedNewlines(parsed.end_marker, kEndMarkerKey, source);

  // Identical markers describe an envelope whose opening line is also its closing line: no body, and no way to
  // terminate. Refusing it here is what keeps the arbiter's grammar unambiguous.
  if (parsed.start_marker == parsed.end_marker) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT, Describe(source), " 'start_marker' and 'end_marker' must differ");
  }

  encoding = std::move(parsed);
}

bool IsBlank(const std::string& text) {
  return text.find_first_not_of(" \t\r\n") == std::string::npos;
}

}  // namespace

void to_json(nlohmann::json& j, const RawEnvelopeEncoding& encoding) {
  j = nlohmann::json{{kTypeKey, kRawEnvelopeEncodingType},
                     {kToolNameKey, encoding.tool_name},
                     {kStartMarkerKey, encoding.start_marker},
                     {kEndMarkerKey, encoding.end_marker}};
}

void from_json(const nlohmann::json& j, RawEnvelopeEncoding& encoding) {
  ValidateInto(j, encoding, /*source=*/{});
}

std::optional<RawEnvelopeEncoding> ParseToolOutputEncoding(const std::string& text, std::string_view source) {
  if (IsBlank(text)) {
    return std::nullopt;
  }

  nlohmann::json parsed;

  try {
    parsed = nlohmann::json::parse(text);
  } catch (const nlohmann::json::exception& e) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT, Describe(source), " is not valid JSON: ", e.what());
  }

  RawEnvelopeEncoding encoding;
  ValidateInto(parsed, encoding, source);

  return encoding;
}

}  // namespace fl
