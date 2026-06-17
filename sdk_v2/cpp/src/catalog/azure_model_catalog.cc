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
  // In cache-only mode, read only from the disk cache file — no network calls, no local model scanning.
  // The cache file already includes local models from the last full catalog refresh by the long-running service
  // process.
  // TODO: For our CLI usage the catalog file would be current as we use an ephemeral port for the web service and
  // therefore have to run FL first to acquire the external URL value, and that run would have updated the cached
  // catalog info.
  // If someone had a hardcoded web service URL they were using that sequence of events isn't guaranteed. If we care,
  // we could update 'cache_only_' mode to enable refreshing the cache info if it is old. The cache file has a
  // savedAtUnix timestamp property that can be used.
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
  auto local_models = ScanLocalModelInfos(cache_dir, logger_);

  logger_.Log(LogLevel::Information,
              fmt::format("Found {} locally cached models.", local_models.size()));

  auto fetch_from = [&](const std::string& url, const std::optional<std::string>& filter) {
    // Preserve byte-identical behavior for the "no override" case (previously stored as ""),
    // while letting callers explicitly request "" as a real filter override.
    auto client = MakeCatalogClient(url, filter.value_or(""), ep_detector_, logger_, cache_dir);
    auto model_infos = FetchAllModelInfosWithCachedModels(*client, local_models, logger_);

    for (const auto& info : model_infos) {
      // Check if the model is locally cached and pass the path if so.
      std::string local_path;
      auto it = local_models.find(info.model_id);
      if (it != local_models.end()) {
        local_path = it->second.path;
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
