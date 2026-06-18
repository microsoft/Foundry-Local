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

/// One page of model infos returned by `ICatalogClient::FetchAllVersionsByAlias`.
/// `next_continuation_token` is empty when the underlying source is exhausted;
/// otherwise callers pass it back to retrieve the next page.
struct PagedModelInfos {
  std::vector<ModelInfo> models;
  std::string next_continuation_token;
};
/// Abstract catalog client. Implemented by the live Azure catalog client,
/// which queries the Azure Foundry catalog REST API.
class ICatalogClient {
 public:
  virtual ~ICatalogClient() = default;

  /// Fetch all model infos available from this catalog source. May filter on
  /// available execution providers.
  virtual std::vector<ModelInfo> FetchAllModelInfos() = 0;

  /// Look up specific model IDs (e.g., older versions present in the local
  /// cache that aren't in the latest catalog). Returns {} for IDs that cannot
  /// be resolved.
  virtual std::vector<ModelInfo> FetchModelsByIds(
      const std::vector<std::string>& model_ids) = 0;

  /// Fetch all known versions of a model (by alias), bypassing the "latest only"
  /// filter that `FetchAllModelInfos` applies. Maps to C#
  /// `IAzureFoundryApiService.FetchAllModelVersionsAsync`.
  ///
  /// `model_alias` is optional — when empty, implementations may return all
  /// available versioned models (still subject to device/EP filtering).
  /// `max_versions` is a soft upper bound on the number of variants to return
  /// (0 or negative = no cap). `continuation_token` is an opaque cursor from a
  /// previous call used to resume pagination; empty starts from the beginning.
  /// On return, `PagedModelInfos::next_continuation_token` is set when more
  /// data is available; an empty value means the underlying source has been
  /// fully walked. Implementations that cannot list older versions return
  /// whatever they have locally with an empty token.
  ///
  /// Provided with a default `{}` body so an implementation that has not yet
  /// overridden it still compiles.
  virtual PagedModelInfos FetchAllVersionsByAlias(
      const std::string& /*model_alias*/,
      int /*max_versions*/ = 0,
      const std::string& /*continuation_token*/ = {}) {
    return {};
  }
};

/// Production helper that combines a catalog fetch with locally cached model
/// resolution and BYO synthesis.
std::vector<ModelInfo> FetchAllModelInfosWithCachedModels(
    ICatalogClient& client,
    const std::vector<std::string>& cached_model_ids,
    ILogger& logger);

/// Construct a client for the live Azure Foundry catalog.
/// - `ep_detector` limits results to models supported by this machine.
/// - `filter_override` sets the foundryLocal tag filter.
/// - `catalog_region` controls regional routing: empty/"auto" means detect it,
///   any other value is an explicit region.
/// - `disable_region_fallback` disables cross-region retries.
std::unique_ptr<ICatalogClient> MakeCatalogClient(
    const std::string& base_url,
    const std::string& filter_override,
    const IEpDetector& ep_detector,
    ILogger& logger,
    const std::string& cache_directory,
    const std::string& catalog_region = "",
    bool disable_region_fallback = false);

}  // namespace fl
