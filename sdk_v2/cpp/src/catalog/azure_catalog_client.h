// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "catalog/azure_catalog_models.h"
#include "catalog/catalog_client.h"
#include "ep_detection/ep_detector.h"
#include "http/http_client.h"
#include "logger.h"
#include "model_info.h"
#include "util/region_fallback.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace fl {

/// Live Azure Foundry catalog client. Queries the catalog REST API
/// (`{base_url}/entities/crossRegion`) for the models available to the local
/// hardware, paginating through results and converting each entry to ModelInfo.
///
/// Uses one filter set per detected device, page size 50, and pagination via
/// skip + continuationToken.
class AzureCatalogClient : public ICatalogClient {
 public:
  /// Response-aware HTTP POST. Used for region detection, catalog fetches, and regional fallback.
  using HttpPostResponseFn =
      std::function<http::HttpResponse(const std::string& url, const std::string& body)>;

  /// @param base_url Catalog base URL, e.g. "https://ai.azure.com/api/eastus/ux/v1.0".
  /// @param filter_override Foundry Local tag filter. "" means public models; "''" means a single empty value.
  /// @param ep_detector Reports available device and execution-provider pairs.
  /// @param logger Logger.
  /// @param http_post HTTP POST implementation. The default uses `http::HttpPostWithResponse`.
  /// @param catalog_region Catalog region. Empty or "auto" detects from Azure headers; any other value is explicit.
  /// @param region_fallback_enabled Enables retries through nearby regions when a regional endpoint is unhealthy.
  AzureCatalogClient(const std::string& base_url,
                     const std::string& filter_override,
                     const IEpDetector& ep_detector,
                     ILogger& logger,
                     HttpPostResponseFn http_post = {},
                     std::string catalog_region = "",
                     bool region_fallback_enabled = true);

  /// Fetch every catalog model entry visible to the local hardware (raw form,
  /// before conversion to ModelInfo). One filter set per device, fully paginated.
  std::vector<CatalogLocalModel> FetchAllModels();

  std::vector<ModelInfo> FetchAllModelInfos() override;

  std::vector<ModelInfo> FetchModelsByIds(const std::vector<std::string>& model_ids) override;

  /// Fetch every version of `model_alias` from the live catalog by issuing the
  /// per-device search with `labels=latest` removed and an alias filter added.
  /// When `model_alias` is empty the alias filter is also omitted, so this
  /// returns every versioned model the local hardware can run, across all of
  /// their versions.
  /// Honours `max_versions` as a soft upper bound and `continuation_token` as
  /// an opaque cursor. Each call returns up to `max_versions` items from a
  /// single logical position in the upstream stream; a non-empty
  /// `next_continuation_token` is set when more data is available.
  PagedModelInfos FetchAllVersionsByAlias(const std::string& model_alias,
                                          int max_versions = 0,
                                          const std::string& continuation_token = {}) override;

 private:
  /// Per-page result of walking one filter set with explicit pagination state.
  struct FilterSetWalk {
    std::vector<CatalogLocalModel> models;
    std::string region;                          // pinned region that served the page(s)
    std::optional<int> next_skip;                // server's next skip (when not done)
    std::optional<std::string> next_inner_token; // server's next continuation token
    bool done = false;     // true when the underlying stream is exhausted
    bool aborted = false;  // true when a retryable region failure happened
  };

  /// Walks pages of one filter set with explicit pagination state.
  /// - `skip` / `inner_token`: starting cursor (nullopt for a fresh start).
  /// - `region_in`: pinned region for resuming (empty triggers page-1 region fallback).
  /// - `max_count`: soft cap; 0 means walk to exhaustion.
  /// On success, returns `done=true` if the stream was exhausted, or `done=false`
  /// with `next_skip`/`next_inner_token` populated when stopped by the cap.
  /// On a retryable region failure, returns `aborted=true` (partial results discarded).
  FilterSetWalk FetchFilterSetWithState(const std::vector<CatalogFilter>& filters,
                                        std::optional<int> skip,
                                        std::optional<std::string> inner_token,
                                        std::string region_in,
                                        int max_count);

  /// Fetch every device filter set, dropping the ones that failed their region-health checks.
  /// Used for unbounded "latest only" / by-id queries.
  std::vector<FilterSetWalk> FetchAllFilterSets();

  std::string base_url_;
  std::vector<std::string> model_filter_;  // foundryLocal tag values
  const IEpDetector& ep_detector_;
  ILogger& logger_;
  HttpPostResponseFn http_post_response_;
  RegionFallback region_fallback_;

  // Region state. region_ is the active region (empty = use base_url verbatim).
  // When base_url matches the https://{host}/api/{region}/{suffix} template,
  // url_prefix_/url_suffix_ let us synthesize per-region URLs.
  std::string region_;
  std::string url_prefix_;
  std::string url_suffix_;
  bool regional_template_ = false;
};

}  // namespace fl
