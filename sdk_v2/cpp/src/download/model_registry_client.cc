// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "download/model_registry_client.h"
#include "exception.h"
#include "http/http_client.h"

#include <nlohmann/json.hpp>

#include <string>

namespace fl {
namespace {

/// Build the model registry base URL for the given Azure region.
/// Format: https://{region}.api.azureml.ms/modelregistry/v1.0/registry/models/nonazureaccount?assetId=
std::string BuildBaseUrl(const std::string& region) {
  return "https://" + region + ".api.azureml.ms/modelregistry/v1.0/registry/models/nonazureaccount?assetId=";
}

/// URL-encode a string (percent-encoding).
std::string UrlEncode(const std::string& value) {
  std::string result;
  result.reserve(value.size() * 2);
  for (unsigned char c : value) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '~') {
      result += static_cast<char>(c);
    } else {
      static const char hex[] = "0123456789ABCDEF";
      result += '%';
      result += hex[c >> 4];
      result += hex[c & 0x0F];
    }
  }
  return result;
}

constexpr const char* kUserAgent = "AzureAiStudio";

std::string DefaultHttpGet(const std::string& url) {
  return http::HttpGet(url, kUserAgent);
}

}  // anonymous namespace

ModelRegistryClient::ModelRegistryClient(std::string region)
    : base_url_(BuildBaseUrl(region)), http_get_(DefaultHttpGet) {}

void ModelRegistryClient::SetHttpGet(HttpGetFn fn) {
  http_get_ = std::move(fn);
}

ModelContainer ModelRegistryClient::ResolveModelContainer(const std::string& asset_id) {
  if (asset_id.empty()) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL, "cannot resolve model container: empty asset_id");
  }

  std::string url = base_url_ + UrlEncode(asset_id);
  std::string response_str = http_get_(url);

  if (response_str.empty()) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL, "empty response from model registry API for asset_id: " + asset_id);
  }

  try {
    auto j = nlohmann::json::parse(response_str);
    ModelContainer result;

    if (j.contains("blobSasUri") && j["blobSasUri"].is_string()) {
      result.blob_sas_uri = j["blobSasUri"].get<std::string>();
    } else {
      FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL, "model registry response missing blobSasUri for asset_id: " + asset_id);
    }

    if (j.contains("modelEntity") && j["modelEntity"].is_object()) {
      auto& me = j["modelEntity"];
      if (me.contains("description") && me["description"].is_string()) {
        result.description = me["description"].get<std::string>();
      }
    }

    return result;
  } catch (const nlohmann::json::exception& e) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
             std::string("failed to parse model registry response: ") + e.what());
  }
}

}  // namespace fl
