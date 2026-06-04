// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "catalog/catalog_client.h"
#include "ep_detection/ep_detector.h"
#include "utils.h"

#include <foundry_local/foundry_local_c.h>

#include <fmt/format.h>

#include <memory>
#include <set>

namespace fl {

// Implemented in static_catalog_client.cc (always linked).
std::unique_ptr<ICatalogClient> MakeStaticCatalogClient(
    const IEpDetector& ep_detector, ILogger& logger);

// Implemented in azure_catalog_client.cc (private repo only).
std::unique_ptr<ICatalogClient> MakeLiveCatalogClient(
    const std::string& base_url,
    const std::string& filter_override,
    const IEpDetector& ep_detector,
    ILogger& logger,
    const std::string& cache_directory);

std::unique_ptr<ICatalogClient> MakeCatalogClient(
    const std::string& base_url,
    const std::string& filter_override,
    const IEpDetector& ep_detector,
    ILogger& logger,
    const std::string& cache_directory) {
  if (base_url == "static") {
    if (!filter_override.empty()) {
      logger.Log(LogLevel::Information,
                 fmt::format("static catalog: ignoring filter '{}'", filter_override));
    }

    return MakeStaticCatalogClient(ep_detector, logger);
  }

  return MakeLiveCatalogClient(base_url, filter_override, ep_detector, logger, cache_directory);
}

std::vector<ModelInfo> FetchAllModelInfosWithCachedModels(
    ICatalogClient& client,
    const std::vector<std::string>& cached_model_ids,
    ILogger& logger) {
  std::map<std::string, LocalModelScanResult> cached_models;
  for (const auto& id : cached_model_ids) {
    cached_models[id] = {};
  }

  return FetchAllModelInfosWithCachedModels(client, cached_models, logger);
}

std::vector<ModelInfo> FetchAllModelInfosWithCachedModels(
    ICatalogClient& client,
    const std::map<std::string, LocalModelScanResult>& cached_models,
    ILogger& logger) {
  // Step 1: Fetch latest catalog models (existing flow).
  auto result = client.FetchAllModelInfos();

  if (cached_models.empty()) {
    return result;
  }

  // Step 2: Find which cached model IDs are already in the catalog results.
  std::set<std::string> resolved_ids;
  for (const auto& info : result) {
    resolved_ids.insert(info.model_id);
  }

  std::vector<std::string> unresolved_ids;
  for (const auto& cached_model : cached_models) {
    const auto& id = cached_model.first;
    if (resolved_ids.find(id) == resolved_ids.end()) {
      unresolved_ids.push_back(id);
    }
  }

  // Step 3: Look up unresolved IDs from the catalog (older versions, etc.)
  if (!unresolved_ids.empty()) {
    try {
      auto additional = client.FetchModelsByIds(unresolved_ids);
      for (auto& info : additional) {
        resolved_ids.insert(info.model_id);
        result.push_back(std::move(info));
      }
    } catch (const std::exception& ex) {
      logger.Log(LogLevel::Warning,
                 fmt::format("catalog: failed to fetch cached model IDs — {}", ex.what()));
    } catch (...) {
      logger.Log(LogLevel::Warning, "catalog: failed to fetch cached model IDs — unknown error");
    }

    // Step 4: Create basic entries for any IDs still unresolved (BYO models).
    for (const auto& id : unresolved_ids) {
      if (resolved_ids.find(id) != resolved_ids.end()) {
        continue;
      }

      auto [name, version] = Utils::SplitModelNameAndVersion(id);

      ModelInfo info;
      info.model_id = id;
      info.name = name;
      info.alias = name;
      info.uri = "local://" + name;
      info.version = version;
      info.string_properties[FOUNDRY_LOCAL_MODEL_PROP_MODEL_PROVIDER_STR] = "Local";
      info.string_properties[FOUNDRY_LOCAL_MODEL_PROP_MODEL_TYPE_STR] = "ONNX";
      const auto metadata = cached_models.find(id);
      if (metadata != cached_models.end()) {
        if (!metadata->second.tool_call_start.empty()) {
          info.string_properties[FOUNDRY_LOCAL_MODEL_PROP_TOOL_CALL_START_STR] = metadata->second.tool_call_start;
        }

        if (!metadata->second.tool_call_end.empty()) {
          info.string_properties[FOUNDRY_LOCAL_MODEL_PROP_TOOL_CALL_END_STR] = metadata->second.tool_call_end;
        }

        if (metadata->second.supports_tool_calling.has_value()) {
          info.int_properties[FOUNDRY_LOCAL_MODEL_PROP_SUPPORTS_TOOL_CALLING_INT] =
              *metadata->second.supports_tool_calling;
        }
      }

      result.push_back(std::move(info));
    }
  }

  return result;
}

}  // namespace fl
