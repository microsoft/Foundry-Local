// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "catalog/catalog_cache.h"
#include "catalog/catalog_client.h"
#include "catalog/catalog_snapshot_data.h"
#include "ep_detection/ep_detector.h"
#include "logger.h"
#include "model_info.h"
#include "utils.h"

#include <fmt/format.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fl {

std::unique_ptr<ICatalogClient> MakeStaticCatalogClient(
    const IEpDetector& ep_detector, ILogger& logger);

namespace {

class StaticCatalogClient : public ICatalogClient {
 public:
  StaticCatalogClient(const IEpDetector& ep_detector, ILogger& logger)
      : ep_detector_(ep_detector), logger_(logger) {}

  std::vector<ModelInfo> FetchAllModelInfos() override {
    std::string_view json{
        reinterpret_cast<const char*>(kCatalogSnapshotJson),
        kCatalogSnapshotJsonSize};

    auto parsed = ParseCatalogSnapshot(json, "embedded snapshot", logger_);
    if (!parsed) {
      logger_.Log(LogLevel::Warning, "static catalog: embedded snapshot failed to parse");
      return {};
    }

    auto models = std::move(*parsed);

    // Build the set of allowed (device, EP) pairs (lowercased) from the
    // detector. OpenVINO can target CPU/GPU/NPU but only NPU-capable machines
    // should see the NPU variants — match on the pair, not on EP alone.
    const auto& devices_to_eps = ep_detector_.GetAvailableDevicesToEPs();
    std::set<std::pair<std::string, std::string>> allowed;
    for (const auto& [device, eps] : devices_to_eps) {
      for (const auto& ep : eps) {
        allowed.emplace(to_lower(device), to_lower(ep));

        // CudaPluginExecutionProvider is the ORT registration name for the
        // downloadable CUDA plugin EP, but catalog models are tagged with
        // CudaExecutionProvider. Add the canonical name as an alias so
        // plugin-EP machines can see and load CUDA catalog models.
        if (to_lower(ep) == "cudapluginexecutionprovider") {
          allowed.emplace(to_lower(device), "cudaexecutionprovider");
        }
      }
    }

    auto removed = std::remove_if(models.begin(), models.end(),
                                  [&](const ModelInfo& m) {
                                    auto device_str = DeviceTypeToString(m.device_type);
                                    return allowed.count({to_lower(device_str), to_lower(m.execution_provider)}) == 0;
                                  });
    models.erase(removed, models.end());

    return models;
  }

  std::vector<ModelInfo> FetchModelsByIds(
      const std::vector<std::string>& /*model_ids*/) override {
    // The snapshot is a fixed point in time; we cannot resolve arbitrary
    // older versions. Return empty so the caller falls through to BYO synthesis.
    return {};
  }

 private:
  const IEpDetector& ep_detector_;
  ILogger& logger_;
};

}  // namespace

std::unique_ptr<ICatalogClient> MakeStaticCatalogClient(
    const IEpDetector& ep_detector, ILogger& logger) {
  return std::make_unique<StaticCatalogClient>(ep_detector, logger);
}

}  // namespace fl
