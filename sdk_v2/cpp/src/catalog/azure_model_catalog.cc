// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "catalog/azure_model_catalog.h"
#include "catalog/catalog_cache.h"
#include "catalog/catalog_client.h"
#include "catalog/local_model_scanner.h"
#include "model.h"
#include "model_info.h"
#include "utils.h"

#include <foundry_local/foundry_local_c.h>
#include <fmt/format.h>

namespace fl {

AzureModelCatalog::AzureModelCatalog(std::vector<std::pair<std::string, std::optional<std::string>>> catalog_urls,
                                     std::string cache_dir,
                                     ModelFactory model_factory,
                                     const IEpDetector& ep_detector,
                                     ILogger& logger,
                                     bool cache_only)
    : BaseModelCatalog(catalog_urls.empty() ? kDefaultCatalogUrl : catalog_urls.front().first, logger),
      catalog_urls_(std::move(catalog_urls)),
      cache_dir_(std::move(cache_dir)),
      model_factory_(std::move(model_factory)),
      ep_detector_(ep_detector),
      logger_(logger),
      cache_only_(cache_only) {
  logger_.Log(LogLevel::Information,
              fmt::format("Created AzureModelCatalog. Cache directory: {}",
                          cache_dir_));
}

AzureModelCatalog::~AzureModelCatalog() = default;

std::vector<Model> AzureModelCatalog::FetchModels() const {
  // In cache-only mode, read only from the disk cache file — no network calls,
  // no local model scanning. The cache file already includes local models from
  // the last full catalog refresh by the long-running service process.
  if (cache_only_) {
    CatalogCache cache(cache_dir_, logger_);
    cache.Load();
    auto cached = cache.GetCachedModels();

    std::vector<Model> models;

    if (cached) {
      for (const auto& info : *cached) {
        models.push_back(model_factory_(ModelInfo(info), /*local_path=*/""));
      }
    }

    logger_.Log(LogLevel::Information,
                fmt::format("Cache-only mode: populated {} models from cache file.", models.size()));

    return models;
  }

  std::vector<Model> models;
  const std::string& cache_dir = cache_dir_;

  logger_.Log(LogLevel::Information,
              "Getting latest info from the Azure catalog and for locally cached models.");

  // Discover locally cached models.
  auto local_models = ScanLocalModels(cache_dir, logger_);
  std::vector<std::string> cached_model_ids;
  cached_model_ids.reserve(local_models.size());
  for (const auto& [id, path] : local_models) {
    cached_model_ids.push_back(id);
  }

  logger_.Log(LogLevel::Information,
              fmt::format("Found {} locally cached models.", cached_model_ids.size()));

  auto fetch_from = [&](const std::string& url, const std::optional<std::string>& filter) {
    // Preserve byte-identical behavior for the "no override" case (previously stored as ""),
    // while letting callers explicitly request "" as a real filter override.
    auto client = MakeCatalogClient(url, filter.value_or(""), ep_detector_, logger_, cache_dir);
    auto model_infos = FetchAllModelInfosWithCachedModels(*client, cached_model_ids, logger_);

    for (const auto& info : model_infos) {
      // Check if the model is locally cached and pass the path if so.
      std::string local_path;
      auto it = local_models.find(info.model_id);
      if (it != local_models.end()) {
        local_path = it->second;
      }

      models.push_back(model_factory_(ModelInfo(info), std::move(local_path)));
    }
  };

  if (catalog_urls_.empty()) {
    // Use default Azure Foundry catalog
    try {
      fetch_from(kDefaultCatalogUrl, kDefaultCatalogFilter);
    } catch (const std::exception& ex) {
      logger_.Log(LogLevel::Error,
                  fmt::format("failed to fetch catalog from default URL: {}", ex.what()));
    }
  } else {
    for (const auto& [url, filter] : catalog_urls_) {
      try {
        fetch_from(url, filter);
      } catch (const std::exception& ex) {
        // One failing URL shouldn't block others — skip and continue.
        logger_.Log(LogLevel::Error,
                    fmt::format("failed to fetch catalog from {}: {}", url, ex.what()));
      }
    }
  }

  logger_.Log(LogLevel::Information,
              fmt::format("Populated model info for {} models.", models.size()));

  return models;
}

}  // namespace fl
