// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "contracts/embeddings.h"

namespace fl {

void from_json(const nlohmann::json& j, EmbeddingCreateRequest& r) {
  r.model = j.at("model").get<std::string>();

  // "input" can be a string or array of strings
  const auto& input_field = j.at("input");
  if (input_field.is_string()) {
    r.input = input_field.get<std::string>();
  } else if (input_field.is_array()) {
    r.input = input_field.get<std::vector<std::string>>();
  } else {
    throw nlohmann::json::type_error::create(302, "\"input\" must be a string or array of strings", &j);
  }

  if (j.contains("encoding_format") && j["encoding_format"].is_string()) {
    r.encoding_format = j["encoding_format"].get<std::string>();
  }
}

void to_json(nlohmann::json& j, const EmbeddingData& d) {
  j = nlohmann::json{
      {"object", d.object},
      {"embedding", d.embedding},
      {"index", d.index},
  };
}

void to_json(nlohmann::json& j, const EmbeddingUsage& u) {
  j = nlohmann::json{
      {"prompt_tokens", u.prompt_tokens},
      {"total_tokens", u.total_tokens},
  };
}

void to_json(nlohmann::json& j, const EmbeddingCreateResponse& r) {
  j = nlohmann::json{
      {"object", r.object},
      {"model", r.model},
      {"data", r.data},
      {"usage", r.usage},
  };
}

}  // namespace fl
