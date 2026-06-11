// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "catalog/base_model_catalog.h"
#include "ep_detection/ep_detector.h"
#include "logger.h"

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace fl {

/// Azure-specific catalog. Fetches from Azure Foundry catalog API,
/// scans local cache, merges results.
/// Maps to C# AzureModelCatalog.
class AzureModelCatalog : public BaseModelCatalog {
 public:
  using ModelFactory = std::function<Model(ModelInfo info, std::string local_path)>;

  AzureModelCatalog(std::vector<std::pair<std::string, std::optional<std::string>>> catalog_urls,
                    std::string cache_dir,
                    ModelFactory model_factory,
                    const IEpDetector& ep_detector,
                    ILogger& logger,
                    bool cache_only = false);
  ~AzureModelCatalog() override;

 protected:
  std::vector<Model> FetchModels() const override;
  std::vector<Model> FetchModelVersions(const std::string& model_alias) const override;
  std::vector<Model> FetchModelsByIds(const std::vector<std::string>& model_ids) const override;

 private:
#if defined(FOUNDRY_LOCAL_HAVE_LIVE_CATALOG_CLIENT)
  static constexpr const char* kDefaultCatalogUrl = "https://ai.azure.com/api/eastus/ux/v1.0";
#else
  // Live Azure catalog client was excluded from this build; default to the
  // embedded snapshot so the SDK works out of the box without a custom config.
  static constexpr const char* kDefaultCatalogUrl = "static";
#endif
  static constexpr const char* kDefaultCatalogFilter = "''";

  std::vector<std::pair<std::string, std::optional<std::string>>> catalog_urls_;
  std::string cache_dir_;
  ModelFactory model_factory_;
  const IEpDetector& ep_detector_;
  ILogger& logger_;
  bool cache_only_;
};

}  // namespace fl
