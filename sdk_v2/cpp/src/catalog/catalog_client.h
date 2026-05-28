// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "ep_detection/ep_detector.h"
#include "logger.h"
#include "model_info.h"

#include <memory>
#include <string>
#include <vector>

namespace fl {

/// Abstract catalog client. Two implementations exist: the live Azure catalog
/// client (private repo) and a snapshot-based static client (public repo).
class ICatalogClient {
 public:
  virtual ~ICatalogClient() = default;

  /// Fetch all model infos available from this catalog source. May filter on
  /// available execution providers.
  virtual std::vector<ModelInfo> FetchAllModelInfos() = 0;

  /// Look up specific model IDs (e.g., older versions present in the local
  /// cache that aren't in the latest catalog). Implementations that have no
  /// way to resolve arbitrary IDs (e.g., the static snapshot client) return {}.
  virtual std::vector<ModelInfo> FetchModelsByIds(
      const std::vector<std::string>& model_ids) = 0;
};

/// Production helper that combines a catalog fetch with locally cached model
/// resolution and BYO synthesis.
std::vector<ModelInfo> FetchAllModelInfosWithCachedModels(
    ICatalogClient& client,
    const std::vector<std::string>& cached_model_ids,
    ILogger& logger);

/// Construct a catalog client. Dispatches based on `base_url`:
/// - "static" -> returns a client backed by the embedded snapshot. Ignores
///   `filter_override` and `cache_directory`. Filters models by the
///   (device, execution_provider) pairs reported by `ep_detector`.
/// - anything else -> returns the live Azure catalog client.
///
/// "static" is a temporary magic value to enable the public-repo build that
/// ships without the live client implementation.
std::unique_ptr<ICatalogClient> MakeCatalogClient(
    const std::string& base_url,
    const std::string& filter_override,
    const IEpDetector& ep_detector,
    ILogger& logger,
    const std::string& cache_directory);

}  // namespace fl
