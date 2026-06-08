// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "contracts/audio_transcriptions.h"

#include "util/json_helpers.h"

namespace fl {

// ========================================================================
// Request deserialization (from_json)
// ========================================================================

void from_json(const nlohmann::json& j, AudioTranscriptionRequest& r) {
  // Required fields
  r.model = j.at("model").get<std::string>();
  r.filename = j.at("filename").get<std::string>();

  // Optional scalar fields
  opt_str(j, "language", r.language);
  opt_float(j, "temperature", r.temperature);
  opt_str(j, "response_format", r.response_format);
  opt_str(j, "prompt", r.prompt);
  opt_bool(j, "stream", r.stream);
}

// ========================================================================
// Response serialization (to_json)
// ========================================================================

void to_json(nlohmann::json& j, const AudioTranscriptionResponse& r) {
  j = nlohmann::json{
      {"text", r.text},
  };

  if (!r.id.empty()) {
    j["id"] = r.id;
  }
}

}  // namespace fl
