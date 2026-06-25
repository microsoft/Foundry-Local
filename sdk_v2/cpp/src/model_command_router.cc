// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "model_command_router.h"

#include "exception.h"
#include "http/http_client.h"
#include "inferencing/model_load_manager.h"
#include "logger.h"
#include "utils.h"

#include <foundry_local/foundry_local_c.h>
#include <nlohmann/json.hpp>

#include <algorithm>

namespace fl {

namespace {

// Model load can be slow (cold cache, large weights); use a deliberate, generous client-side
// timeout instead of the v1 bindings' inconsistent/accidental values. List and unload are
// quick metadata operations and use the default request timeout.
constexpr std::chrono::minutes kLoadTimeout{5};
constexpr std::chrono::seconds kDefaultTimeout{30};

}  // namespace

ModelCommandRouter::ModelCommandRouter(std::optional<std::string> external_service_url,
                                       ModelLoadManager& load_manager,
                                       std::string app_name,
                                       ILogger& logger)
    : external_service_url_(std::move(external_service_url)),
      load_manager_(load_manager),
      user_agent_("foundry-local-sdk/" + app_name),
      logger_(logger) {}

std::string ModelCommandRouter::BaseUrl() const {
  std::string base = *external_service_url_;

  // The configured URL may or may not carry a trailing '/'; normalise so the appended
  // "/models/..." segment never produces a doubled slash.
  if (!base.empty() && base.back() == '/') {
    base.pop_back();
  }

  return base;
}

std::string ModelCommandRouter::ExternalGet(const std::string& context, const std::string& url,
                                            std::chrono::milliseconds timeout) const {
  http::HttpRequestOptions options;
  options.user_agent = user_agent_;
  options.timeout = timeout;

  // Trace the outbound call so external-mode routing is observable; local calls are not logged.
  logger_.Log(LogLevel::Debug, context + " -> GET " + url);

  auto response = http::HttpGetWithResponse(url, options);

  // status == 0 is a transport failure; any non-2xx is a server-side failure. Both are fatal.
  if (response.status < 200 || response.status >= 300) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_NETWORK,
             context + " via " + *external_service_url_ + ": " + http::DescribeFailure(response));
  }

  return response.body;
}

void ModelCommandRouter::Load(std::string_view model_id, std::string_view local_path, ExecutionProvider ep) {
  if (!external_service_url_.has_value()) {
    // LoadModel is idempotent — kModelAlreadyLoaded if the id is already in the map.
    auto result = load_manager_.LoadModel(local_path, model_id, ep);

    if (result.status == ModelLoadManager::LoadStatus::kModelNotFound) {
      FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL, "model not found at path: " + std::string(local_path));
    }

    return;
  }

  // In external mode the remote service loads by id alone; local_path and ep are intentionally
  // not forwarded (the service resolves the path and execution provider itself).
  ExternalGet("load " + std::string(model_id),
              BaseUrl() + "/models/load/" + UrlEncode(model_id),
              kLoadTimeout);
}

void ModelCommandRouter::Unload(std::string_view model_id) {
  if (!external_service_url_.has_value()) {
    // UnloadModel is idempotent — returns false if the id isn't loaded; the bool is ignored.
    load_manager_.UnloadModel(model_id);
    return;
  }

  ExternalGet("unload " + std::string(model_id),
              BaseUrl() + "/models/unload/" + UrlEncode(model_id),
              kDefaultTimeout);
}

bool ModelCommandRouter::IsLoaded(std::string_view model_id) {
  if (!external_service_url_.has_value()) {
    return load_manager_.GetLoadedModel(model_id) != nullptr;
  }

  auto ids = ListLoadedModelIds();
  return std::find(ids.begin(), ids.end(), model_id) != ids.end();
}

std::vector<std::string> ModelCommandRouter::ListLoadedModelIds() {
  if (!external_service_url_.has_value()) {
    return load_manager_.GetLoadedModelIds();
  }

  auto body = ExternalGet("list loaded models", BaseUrl() + "/models/loaded", kDefaultTimeout);

  // Empty / whitespace-only body means "no models loaded" — not a parse error.
  auto first_non_ws = body.find_first_not_of(" \t\r\n");
  if (first_non_ws == std::string::npos) {
    return {};
  }

  // Surface a malformed/non-array/non-string remote response as FOUNDRY_LOCAL_ERROR_INTERNAL
  // as that means we're returning an invalid response from the SDK's web service (assumably).
  nlohmann::json parsed;

  try {
    parsed = nlohmann::json::parse(body);
  } catch (const nlohmann::json::exception& e) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
             "list loaded models via " + *external_service_url_ + ": malformed response body: " + e.what());
  }

  if (!parsed.is_array()) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
             "list loaded models via " + *external_service_url_ + ": expected a JSON array");
  }

  std::vector<std::string> ids;

  for (const auto& element : parsed) {
    if (!element.is_string()) {
      FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
               "list loaded models via " + *external_service_url_ + ": array element is not a string");
    }

    ids.push_back(element.get<std::string>());
  }

  return ids;
}

}  // namespace fl
