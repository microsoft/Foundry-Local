// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "inferencing/execution_provider.h"

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fl {

class ILogger;
class ModelLoadManager;

// -----------------------------------------------------------------------
// ModelCommandRouter — owns the local-vs-external decision for the three
// model management commands (list-loaded / load / unload) ONLY.
//
// When `external_service_url` is unset the router delegates to the in-process
// ModelLoadManager. When set, it redirects the command to that remote Foundry
// Local service over HTTP. The OpenAI-compatible inference endpoints are out of
// scope — callers reach those directly, never through this router.
//
// Keeping this decision in one façade lets `Model` and the catalog stay
// mode-agnostic, and keeps ModelLoadManager a pure local lifecycle owner.
// -----------------------------------------------------------------------

class ModelCommandRouter {
 public:
  /// `load_manager` and `logger` are non-owning references; the owner (Manager) guarantees
  /// they outlive this router via declaration ordering.
  ModelCommandRouter(std::optional<std::string> external_service_url,
                     ModelLoadManager& load_manager,
                     std::string app_name,
                     ILogger& logger);

  ModelCommandRouter(const ModelCommandRouter&) = delete;
  ModelCommandRouter& operator=(const ModelCommandRouter&) = delete;

  /// Local: ModelLoadManager::LoadModel(local_path, model_id, ep), throwing on kModelNotFound.
  /// External: GET {url}/models/load/{url-encoded id} with a generous timeout.
  void Load(std::string_view model_id, std::string_view local_path, ExecutionProvider ep);

  /// Local: ModelLoadManager::UnloadModel(model_id) (idempotent).
  /// External: GET {url}/models/unload/{url-encoded id}.
  void Unload(std::string_view model_id);

  /// Local: ModelLoadManager::GetLoadedModel(model_id) != nullptr.
  /// External: membership test against ListLoadedModelIds(). Batch callers should use
  /// ListLoadedModelIds() directly to avoid a round-trip per id.
  bool IsLoaded(std::string_view model_id);

  /// Local: ModelLoadManager loaded-map keys.
  /// External: GET {url}/models/loaded, parsed as a JSON array of ids (empty body → {}).
  std::vector<std::string> ListLoadedModelIds();

 private:
  /// Trailing-'/'-trimmed external base URL. Precondition: external_service_url_ has a value.
  std::string BaseUrl() const;

  /// Issue an external GET, throwing FOUNDRY_LOCAL_ERROR_NETWORK on non-2xx / transport failure.
  /// `context` is prefixed to the error message (e.g. "load phi-3").
  std::string ExternalGet(const std::string& context, const std::string& url,
                          std::chrono::milliseconds timeout) const;

  std::optional<std::string> external_service_url_;
  ModelLoadManager& load_manager_;
  std::string user_agent_;
  ILogger& logger_;
};

}  // namespace fl
