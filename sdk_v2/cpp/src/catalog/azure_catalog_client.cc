// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "catalog/azure_catalog_client.h"

#include "http/http_client.h"
#include "utils.h"

#include <azure/core/base64.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <utility>
#include <vector>

namespace fl {

namespace {

constexpr const char* kEntitiesPath = "/entities/crossRegion";
constexpr int kPageSize = 50;

// Region detection probe.
constexpr const char* kRegionProbeUrl = "https://api.catalog.azureml.ms/asset-gallery/v1.0/models";
constexpr const char* kRegionProbeBody = R"({"filters":[],"pageSize":1})";
constexpr const char* kServedByClusterHeader = "azureml-served-by-cluster";
constexpr const char* kDefaultRegion = "centralus";

// The catalog and registry gateways reject requests without this User-Agent (HTTP 400).
constexpr const char* kUserAgent = "AzureAiStudio";

/// Strip all leading/trailing single quotes.
std::string TrimSingleQuotes(const std::string& s) {
  const auto begin = s.find_first_not_of('\'');
  if (begin == std::string::npos) {
    return {};
  }

  const auto end = s.find_last_not_of('\'');
  return s.substr(begin, end - begin + 1);
}

/// Build the values for the foundryLocal tag filter from the override string.
/// Empty override → {""} (public models). Otherwise split on ',', drop entries
/// that are empty after whitespace-trimming, then strip surrounding quotes so a
/// caller can request an explicit empty value via "''".
std::vector<std::string> CreateModelFilter(const std::string& filter_override) {
  if (filter_override.empty()) {
    return {std::string{}};
  }

  std::vector<std::string> values;
  std::size_t start = 0;
  while (start <= filter_override.size()) {
    const auto comma = filter_override.find(',', start);
    const auto count = (comma == std::string::npos) ? std::string::npos : comma - start;
    const auto entry = Trim(filter_override.substr(start, count));

    if (!entry.empty()) {
      values.push_back(TrimSingleQuotes(entry));
    }

    if (comma == std::string::npos) {
      break;
    }

    start = comma + 1;
  }

  return values;
}

CatalogFilter MakeFilter(std::string field, std::vector<std::string> values) {
  CatalogFilter f;
  f.field = std::move(field);
  f.op = "eq";
  f.values = std::move(values);
  return f;
}

/// Extract the region from an `azureml-served-by-cluster` header value such as
/// "vienna-eastus-01" → "eastus". Returns "" if the value doesn't match.
std::string ExtractRegionFromClusterHeader(const std::string& header_value) {
  static const std::regex pattern(R"(vienna-(\w+)-\d+)");
  std::smatch match;
  if (std::regex_search(header_value, match, pattern)) {
    return match[1].str();
  }

  return {};
}

/// Split a catalog URL of the form `https://{host}/api/{region}/{suffix}` into
/// its prefix ("https://{host}/api/") and suffix ("/{suffix}"). Returns false if
/// the URL doesn't match that shape, in which case it must be used verbatim.
bool TryParseRegionalCatalogUrl(const std::string& url, std::string* prefix, std::string* suffix) {
  static const std::string kApiMarker = "/api/";
  const auto api_pos = url.find(kApiMarker);
  if (api_pos == std::string::npos) {
    return false;
  }

  const auto region_start = api_pos + kApiMarker.size();
  const auto region_end = url.find('/', region_start);
  if (region_end == std::string::npos || region_end == region_start) {
    return false;
  }

  *prefix = url.substr(0, region_start);
  *suffix = url.substr(region_end);
  return true;
}

bool UsesRegionalRouting(bool regional_template, const std::string& region) {
  return regional_template && !region.empty();
}

/// Detect the Azure region by POSTing a probe to the catalog gallery and reading
/// the `azureml-served-by-cluster` response header. Returns "centralus" on failure.
std::string DetectRegion(const AzureCatalogClient::HttpPostResponseFn& http_post_response, ILogger& logger) {
  http::HttpResponse response = http_post_response(kRegionProbeUrl, kRegionProbeBody);

  std::string region = kDefaultRegion;
  if (response.status >= 200 && response.status < 300) {
    auto it = response.headers.find(kServedByClusterHeader);
    if (it != response.headers.end()) {
      auto parsed = ExtractRegionFromClusterHeader(it->second);
      if (!parsed.empty()) {
        region = parsed;
      }
    }
  } else {
    logger.Log(LogLevel::Warning,
               "Region detection probe failed (status " + std::to_string(response.status) + "); defaulting to '" +
                   kDefaultRegion + "'.");
  }

  logger.Log(LogLevel::Information, "Detected catalog region: '" + region + "'.");
  return region;
}

std::string BuildRegionalUrl(const std::string& url_prefix, const std::string& url_suffix, const std::string& region) {
  return url_prefix + region + url_suffix + kEntitiesPath;
}

std::string BuildRequestUrl(const std::string& base_url,
                            bool regional,
                            const std::string& region,
                            const std::string& url_prefix,
                            const std::string& url_suffix) {
  if (regional) {
    return BuildRegionalUrl(url_prefix, url_suffix, region);
  }

  return base_url + kEntitiesPath;
}

std::string BuildRequestBody(const std::vector<CatalogFilter>& filters,
                             const std::optional<int>& skip,
                             const std::optional<std::string>& continuation_token,
                             int page_size) {
  AzureCatalogRequest request;
  request.resource_ids.push_back({"azureml", "Registry"});
  request.index_entities_request.filters = filters;
  request.index_entities_request.page_size = page_size;
  request.index_entities_request.skip = skip;
  request.index_entities_request.continuation_token = continuation_token;

  const nlohmann::json body = request;
  return body.dump();
}

std::vector<ModelInfo> ToModelInfos(const std::vector<CatalogLocalModel>& raw_models, const std::string& region) {
  std::vector<ModelInfo> infos;
  for (const auto& model : raw_models) {
    if (auto info = CatalogModelToModelInfo(model)) {
      info->detected_region = region;
      infos.push_back(std::move(*info));
    }
  }

  return infos;
}

std::vector<std::vector<CatalogFilter>> BuildSearchFilters(const IEpDetector& ep_detector,
                                                           const std::vector<std::string>& model_filter) {
  std::vector<std::vector<CatalogFilter>> filter_sets;

  // One filter set per detected device. The catalog API matches on the
  // (device, execution provider) pair, so we keep the EPs grouped by device.
  for (const auto& [device, eps] : ep_detector.GetAvailableDevicesToEPs()) {
    std::vector<CatalogFilter> filters;
    filters.push_back(MakeFilter("type", {"models"}));
    filters.push_back(MakeFilter("kind", {"Versioned"}));
    filters.push_back(MakeFilter("labels", {"latest"}));
    filters.push_back(MakeFilter("annotations/tags/foundryLocal", model_filter));
    filters.push_back(MakeFilter("properties/variantInfo/variantMetadata/device", {ToLower(device)}));
    filters.push_back(MakeFilter("properties/variantInfo/variantMetadata/executionProvider", eps));
    filter_sets.push_back(std::move(filters));
  }

  return filter_sets;
}

/// Build per-device filter sets for an all-versions query.
/// Same shape as `BuildSearchFilters` minus the `labels=latest` filter
/// (so older versions are included). When `model_alias` is non-empty an
/// `annotations/tags/alias` filter scopes the result to that one alias; when
/// empty, no alias filter is added and every versioned model the local hardware
/// can run is returned across all its versions.
std::vector<std::vector<CatalogFilter>> BuildAllVersionsFilters(const IEpDetector& ep_detector,
                                                                const std::vector<std::string>& model_filter,
                                                                const std::string& model_alias) {
  std::vector<std::vector<CatalogFilter>> filter_sets;

  for (const auto& [device, eps] : ep_detector.GetAvailableDevicesToEPs()) {
    std::vector<CatalogFilter> filters;
    filters.push_back(MakeFilter("type", {"models"}));
    filters.push_back(MakeFilter("kind", {"Versioned"}));
    filters.push_back(MakeFilter("annotations/tags/foundryLocal", model_filter));
    if (!model_alias.empty()) {
      filters.push_back(MakeFilter("annotations/tags/alias", {model_alias}));
    }
    filters.push_back(MakeFilter("properties/variantInfo/variantMetadata/device", {ToLower(device)}));
    filters.push_back(MakeFilter("properties/variantInfo/variantMetadata/executionProvider", eps));
    filter_sets.push_back(std::move(filters));
  }

  return filter_sets;
}
std::vector<CatalogFilter> BuildModelIdFilters(const std::vector<std::string>& model_filter,
                                               const std::vector<std::string>& model_ids) {
  // Looking up specific IDs: no labels=latest (we want exact versions) and no
  // device/EP filters (the IDs already pin the variant).
  std::vector<CatalogFilter> filters;
  filters.push_back(MakeFilter("type", {"models"}));
  filters.push_back(MakeFilter("kind", {"Versioned"}));
  filters.push_back(MakeFilter("annotations/tags/foundryLocal", model_filter));
  filters.push_back(MakeFilter("properties/id", model_ids));
  return filters;
}

// ---- Continuation-token codec ----
//
// Format: base64(JSON) of:
//   {"v":1,"d":<device_idx>,"s":<int|null>,"t":"<server_token>","r":"<region>"}
// `d` is the index into the per-device filter sets where pagination resumes.
// `s` and `t` are the server's `skip` and `continuationToken` for the current
// filter set; either or both may be absent. `r` is the pinned region (empty
// when the first page hasn't run region fallback yet).
struct ContinuationState {
  size_t device_idx = 0;
  std::optional<int> skip;
  std::optional<std::string> inner_token;
  std::string region;  // empty = not pinned yet
};

std::string EncodeContinuation(const ContinuationState& state) {
  nlohmann::json j;
  j["v"] = 1;
  j["d"] = state.device_idx;
  if (state.skip) {
    j["s"] = *state.skip;
  } else {
    j["s"] = nullptr;
  }
  if (state.inner_token) {
    j["t"] = *state.inner_token;
  } else {
    j["t"] = "";
  }
  j["r"] = state.region;
  const std::string payload = j.dump();
  std::vector<uint8_t> bytes(payload.begin(), payload.end());
  return Azure::Core::Convert::Base64Encode(bytes);
}

// Returns the decoded state, or nullopt if the token is malformed.
std::optional<ContinuationState> DecodeContinuation(const std::string& token) {
  if (token.empty()) {
    return std::nullopt;
  }
  std::vector<uint8_t> bytes;
  try {
    bytes = Azure::Core::Convert::Base64Decode(token);
  } catch (...) {
    return std::nullopt;
  }
  std::string payload(bytes.begin(), bytes.end());
  nlohmann::json j;
  try {
    j = nlohmann::json::parse(payload);
  } catch (...) {
    return std::nullopt;
  }
  ContinuationState state;
  if (j.contains("d") && j["d"].is_number_unsigned()) {
    state.device_idx = j["d"].get<size_t>();
  }
  if (j.contains("s") && j["s"].is_number_integer()) {
    state.skip = j["s"].get<int>();
  }
  if (j.contains("t") && j["t"].is_string()) {
    auto t = j["t"].get<std::string>();
    if (!t.empty()) {
      state.inner_token = std::move(t);
    }
  }
  if (j.contains("r") && j["r"].is_string()) {
    state.region = j["r"].get<std::string>();
  }
  return state;
}
}  // namespace

AzureCatalogClient::AzureCatalogClient(const std::string& base_url,
                                       const std::string& filter_override,
                                       const IEpDetector& ep_detector,
                                       ILogger& logger,
                                       HttpPostResponseFn http_post,
                                       std::string catalog_region,
                                       bool region_fallback_enabled)
    : base_url_(base_url),
      model_filter_(CreateModelFilter(filter_override)),
      ep_detector_(ep_detector),
      logger_(logger),
      http_post_response_(std::move(http_post)),
      region_fallback_(logger, region_fallback_enabled) {
  if (!http_post_response_) {
    http_post_response_ = [](const std::string& url, const std::string& body) {
      http::HttpRequestOptions options;
      options.user_agent = kUserAgent;
      return http::HttpPostWithResponse(url, body, options);
    };
  }

  // Normalize away a single trailing slash so URL composition is predictable.
  if (!base_url_.empty() && base_url_.back() == '/') {
    base_url_.pop_back();
  }

  regional_template_ = TryParseRegionalCatalogUrl(base_url_, &url_prefix_, &url_suffix_);

  // An explicit region is a hard override. Empty/"auto" means detect the region,
  // but only for Azure URLs that can be rewritten per region.
  const auto normalized_catalog_region = ToLower(catalog_region);
  if (!normalized_catalog_region.empty() && normalized_catalog_region != "auto") {
    region_ = normalized_catalog_region;
  } else if (regional_template_) {
    region_ = DetectRegion(http_post_response_, logger_);
  }
}

AzureCatalogClient::FilterSetWalk AzureCatalogClient::FetchFilterSetWithState(
    const std::vector<CatalogFilter>& filters,
    std::optional<int> skip,
    std::optional<std::string> inner_token,
    std::string region_in,
    int max_count) {
  const bool regional = UsesRegionalRouting(regional_template_, region_);

  FilterSetWalk result;
  result.region = region_in;
  std::optional<std::string> pinned_region;
  if (!region_in.empty()) {
    pinned_region = region_in;  // resuming a previously-pinned filter set
  }

  while (true) {
    int requested_page_size = kPageSize;
    if (max_count > 0) {
      const int still_needed = max_count - static_cast<int>(result.models.size());
      if (still_needed <= 0) {
        // Cap reached at a page boundary on a previous iteration: should not
        // happen because we check after each page below, but be defensive.
        result.next_skip = skip;
        result.next_inner_token = inner_token;
        return result;
      }
      requested_page_size = std::min(still_needed, kPageSize);
    }

    const std::string body = BuildRequestBody(filters, skip, inner_token, requested_page_size);

    http::HttpResponse response;
    if (regional && !pinned_region) {
      // Page 1 fresh start: run region fallback starting from the sticky/active region.
      const std::string start = region_fallback_.StickyRegion().value_or(region_);
      try {
        auto fallback_result = region_fallback_.Execute(start, [&](const std::string& r) {
          return http_post_response_(BuildRegionalUrl(url_prefix_, url_suffix_, r), body);
        });
        response = std::move(fallback_result.response);
        pinned_region = fallback_result.region;
        result.region = fallback_result.region;
        region_ = fallback_result.region;  // bias later filter sets
      } catch (const std::exception& ex) {
        logger_.Log(LogLevel::Warning,
                    std::string("catalog: filter set failed across all regions: ") + ex.what());
        result.aborted = true;
        return result;
      }
    } else if (regional) {
      response = http_post_response_(BuildRegionalUrl(url_prefix_, url_suffix_, *pinned_region), body);
    } else {
      response = http_post_response_(BuildRequestUrl(base_url_, regional, region_, url_prefix_, url_suffix_), body);
    }

    if (response.status == 0 || response.status < 200 || response.status >= 300) {
      if (regional && IsRegionRetryableStatus(response.status)) {
        logger_.Log(LogLevel::Warning,
                    "catalog: filter set failed (" + http::DescribeFailure(response) + "); skipping this filter set.");
        result.aborted = true;
        return result;
      }
      const std::string url = regional && pinned_region
                                  ? BuildRegionalUrl(url_prefix_, url_suffix_, *pinned_region)
                                  : BuildRequestUrl(base_url_, regional, region_, url_prefix_, url_suffix_);
      FL_THROW(FOUNDRY_LOCAL_ERROR_NETWORK,
               "catalog request to " + url + " failed: " + http::DescribeFailure(response));
    }

    const auto parsed = nlohmann::json::parse(response.body).get<AzureCatalogResponse>();
    if (!parsed.index_entities_response) {
      result.done = true;
      return result;
    }

    const auto& page = *parsed.index_entities_response;
    if (page.models.empty()) {
      result.done = true;
      return result;
    }

    result.models.insert(result.models.end(), page.models.begin(), page.models.end());

    // Advance pagination. A non-positive nextSkip and an empty token mean "done".
    skip = (page.next_skip && *page.next_skip > 0) ? page.next_skip : std::nullopt;
    inner_token = (page.continuation_token && !page.continuation_token->empty())
                      ? page.continuation_token
                      : std::nullopt;

    const bool stream_done = !skip && !inner_token;
    if (stream_done) {
      result.done = true;
      return result;
    }

    if (max_count > 0 && static_cast<int>(result.models.size()) >= max_count) {
      // Cap hit and more data is available — emit cursor.
      result.next_skip = skip;
      result.next_inner_token = inner_token;
      return result;
    }
  }
}

std::vector<AzureCatalogClient::FilterSetWalk> AzureCatalogClient::FetchAllFilterSets() {
  std::vector<FilterSetWalk> results;
  for (const auto& filters : BuildSearchFilters(ep_detector_, model_filter_)) {
    auto result = FetchFilterSetWithState(filters, std::nullopt, std::nullopt, std::string{}, /*max_count=*/0);
    if (!result.aborted) {
      results.push_back(std::move(result));
    }
  }

  return results;
}

std::vector<CatalogLocalModel> AzureCatalogClient::FetchAllModels() {
  std::vector<CatalogLocalModel> models;
  for (auto& set : FetchAllFilterSets()) {
    models.insert(models.end(), std::make_move_iterator(set.models.begin()),
                  std::make_move_iterator(set.models.end()));
  }

  return models;
}

std::vector<ModelInfo> AzureCatalogClient::FetchAllModelInfos() {
  std::vector<ModelInfo> infos;
  for (const auto& set : FetchAllFilterSets()) {
    auto batch = ToModelInfos(set.models, set.region);
    infos.insert(infos.end(), std::make_move_iterator(batch.begin()), std::make_move_iterator(batch.end()));
  }

  return infos;
}

std::vector<ModelInfo> AzureCatalogClient::FetchModelsByIds(
    const std::vector<std::string>& model_ids) {
  if (model_ids.empty()) {
    return {};
  }

  auto result = FetchFilterSetWithState(BuildModelIdFilters(model_filter_, model_ids),
                                        std::nullopt, std::nullopt, std::string{}, /*max_count=*/0);
  if (result.aborted) {
    return {};
  }

  return ToModelInfos(result.models, result.region);
}

PagedModelInfos AzureCatalogClient::FetchAllVersionsByAlias(const std::string& model_alias,
                                                            int max_versions,
                                                            const std::string& continuation_token) {
  // Empty alias → list every versioned model the local hardware can run, across
  // all of their versions (i.e. `FetchAllModelInfos` minus the `labels=latest`
  // filter). Non-empty alias → same query, scoped to that alias.
  //
  // Pagination model: each call walks one logical position in the upstream
  // stream and returns up to `max_versions` items. The opaque
  // `continuation_token` encodes which per-device filter set we're in plus the
  // server's (skip, token, region) for that filter set. When `max_versions`
  // is hit mid-set we emit a cursor to resume from the same position; when a
  // set finishes naturally we either continue into the next set (if budget
  // allows) or emit a cursor pointing to the start of the next set.
  const auto filter_sets = BuildAllVersionsFilters(ep_detector_, model_filter_, model_alias);

  ContinuationState in_state;
  if (!continuation_token.empty()) {
    if (auto decoded = DecodeContinuation(continuation_token)) {
      in_state = std::move(*decoded);
    } else {
      logger_.Log(LogLevel::Warning, "catalog: ignoring malformed continuation token; restarting from the beginning.");
    }
  }

  size_t device_idx = in_state.device_idx;
  std::optional<int> skip = in_state.skip;
  std::optional<std::string> inner_token = in_state.inner_token;
  std::string region_in = in_state.region;

  PagedModelInfos result;
  const int cap = max_versions > 0 ? max_versions : 0;  // 0 = unbounded

  while (device_idx < filter_sets.size()) {
    if (cap > 0 && static_cast<int>(result.models.size()) >= cap) {
      break;
    }

    const int remaining = (cap > 0) ? cap - static_cast<int>(result.models.size()) : 0;
    auto walk = FetchFilterSetWithState(filter_sets[device_idx], skip, inner_token, region_in, remaining);

    if (walk.aborted) {
      // Skip this filter set entirely; reset state and advance.
      ++device_idx;
      skip.reset();
      inner_token.reset();
      region_in.clear();
      continue;
    }

    auto batch = ToModelInfos(walk.models, walk.region);
    result.models.insert(result.models.end(),
                         std::make_move_iterator(batch.begin()),
                         std::make_move_iterator(batch.end()));

    if (!walk.done) {
      // Cap was reached mid-filter-set; emit cursor pointing here.
      ContinuationState out;
      out.device_idx = device_idx;
      out.skip = walk.next_skip;
      out.inner_token = walk.next_inner_token;
      out.region = walk.region;
      result.next_continuation_token = EncodeContinuation(out);
      return result;
    }

    // Set complete; advance to the next.
    ++device_idx;
    skip.reset();
    inner_token.reset();
    region_in.clear();
  }

  // Loop exited because either (a) cap was reached on a filter-set boundary, or
  // (b) we walked every filter set. If filter sets remain, encode a cursor that
  // resumes at the start of the next set so the caller sees a stable page size.
  if (cap > 0 && static_cast<int>(result.models.size()) >= cap && device_idx < filter_sets.size()) {
    ContinuationState out;
    out.device_idx = device_idx;
    result.next_continuation_token = EncodeContinuation(out);
  }

  return result;
}

std::unique_ptr<ICatalogClient> MakeCatalogClient(
    const std::string& base_url,
    const std::string& filter_override,
    const IEpDetector& ep_detector,
    ILogger& logger,
    const std::string& /*cache_directory*/,
    const std::string& catalog_region,
    bool disable_region_fallback) {
  return std::make_unique<AzureCatalogClient>(base_url, filter_override, ep_detector, logger,
                                              AzureCatalogClient::HttpPostResponseFn{},
                                              catalog_region, !disable_region_fallback);
}

}  // namespace fl
