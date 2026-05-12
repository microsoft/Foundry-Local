// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Tests for the static catalog client (embedded snapshot path via
// MakeCatalogClient("static")). These tests are independent of the live
// Azure catalog client and must continue to compile when the live client
// source files have been removed from the build.
//
#include <foundry_local/foundry_local_c.h>
#include <gtest/gtest.h>

#include <map>
#include <string>
#include <vector>

#include "catalog/azure_catalog_models.h"
#include "catalog/catalog_client.h"
#include "ep_detection/ep_detector.h"
#include "logger.h"
#include "model_info.h"

using namespace fl;

namespace {

// Returns all models from the static client for a given EP detector.
std::vector<ModelInfo> FetchStaticModels(const IEpDetector& ep_detector) {
  StderrLogger logger;
  auto client = MakeCatalogClient("static", "", ep_detector, logger, "");
  return client->FetchAllModelInfos();
}

// True if every model in `models` passes `predicate`.
template <typename Pred>
bool AllModels(const std::vector<ModelInfo>& models, Pred predicate) {
  for (const auto& m : models) {
    if (!predicate(m)) {
      return false;
    }
  }
  return true;
}

// True if any model in `models` passes `predicate`.
template <typename Pred>
bool AnyModel(const std::vector<ModelInfo>& models, Pred predicate) {
  for (const auto& m : models) {
    if (predicate(m)) {
      return true;
    }
  }
  return false;
}

}  // namespace

// CPU-only machine: only CPU-device models should appear.
TEST(StaticCatalogClientTest, CpuOnly_FiltersOutGpuAndNpuModels) {
  StderrLogger logger;
  StubEpDetector ep;
  auto client = MakeCatalogClient("static", "", ep, logger, "");
  auto models = client->FetchAllModelInfos();

  // The snapshot has models — a CPU-only machine should get at least some.
  EXPECT_GT(models.size(), 0u) << "expected at least one CPU model in the snapshot";

  // Every returned model must target CPU.
  EXPECT_TRUE(AllModels(models, [](const ModelInfo& m) {
    return m.device_type == DeviceType::kCPU;
  })) << "CPU-only detector returned a non-CPU model";
}

// GPU machine (CUDA): should see CPU + CUDA GPU models, not NPU.
TEST(StaticCatalogClientTest, GpuCuda_IncludesCpuAndGpuExcludesNpu) {
  class CpuAndCudaEpDetector : public IEpDetector {
   public:
    std::map<std::string, std::vector<std::string>> GetAvailableDevicesToEPs() const override {
      return {
          {"CPU", {"CPUExecutionProvider"}},
          {"GPU", {"CUDAExecutionProvider"}},
      };
    }
  } ep;

  auto models = FetchStaticModels(ep);

  EXPECT_GT(models.size(), 0u);

  // No NPU models should appear.
  EXPECT_FALSE(AnyModel(models, [](const ModelInfo& m) {
    return m.device_type == DeviceType::kNPU;
  })) << "GPU detector should not return NPU models";

  // Must have at least one GPU model (the snapshot contains CUDA variants).
  EXPECT_TRUE(AnyModel(models, [](const ModelInfo& m) {
    return m.device_type == DeviceType::kGPU;
  })) << "expected at least one GPU model for a CUDA-capable machine";
}

// OpenVINO on CPU only: should not leak NPU variants even though OpenVINO
// supports both CPU and NPU. The (device, EP) pair match prevents this.
TEST(StaticCatalogClientTest, OpenVinoCpuOnly_DoesNotReturnNpuVariants) {
  class OpenVinoCpuEpDetector : public IEpDetector {
   public:
    std::map<std::string, std::vector<std::string>> GetAvailableDevicesToEPs() const override {
      return {
          {"CPU", {"CPUExecutionProvider", "OpenVINOExecutionProvider"}},
      };
    }
  } ep;

  auto models = FetchStaticModels(ep);

  EXPECT_FALSE(AnyModel(models, [](const ModelInfo& m) {
    return m.device_type == DeviceType::kNPU;
  })) << "OpenVINO+CPU-only detector must not return NPU variants";

  EXPECT_FALSE(AnyModel(models, [](const ModelInfo& m) {
    return m.device_type == DeviceType::kGPU;
  })) << "OpenVINO+CPU-only detector must not return GPU variants";
}

// OpenVINO with CPU + NPU: NPU variants should appear, GPU should not.
TEST(StaticCatalogClientTest, OpenVinoCpuAndNpu_ReturnsNpuVariants) {
  class OpenVinoCpuNpuEpDetector : public IEpDetector {
   public:
    std::map<std::string, std::vector<std::string>> GetAvailableDevicesToEPs() const override {
      return {
          {"CPU", {"CPUExecutionProvider", "OpenVINOExecutionProvider"}},
          {"NPU", {"OpenVINOExecutionProvider"}},
      };
    }
  } ep;

  auto models = FetchStaticModels(ep);

  // The snapshot should contain OpenVINO NPU variants; they must appear here.
  // (If the snapshot has none, this assertion still passes — the test is about
  // not incorrectly *excluding* them.)
  EXPECT_FALSE(AnyModel(models, [](const ModelInfo& m) {
    return m.device_type == DeviceType::kGPU;
  })) << "OpenVINO+CPU+NPU detector must not return GPU variants";
}

// Static client never makes network calls — FetchModelsByIds returns empty.
// This ensures BYO model synthesis works correctly via FetchAllModelInfosWithCachedModels.
TEST(StaticCatalogClientTest, FetchModelsByIds_AlwaysReturnsEmpty) {
  StderrLogger logger;
  StubEpDetector ep;
  auto client = MakeCatalogClient("static", "", ep, logger, "");
  auto result = client->FetchModelsByIds({"phi-4-mini:3", "custom-model:0"});
  EXPECT_TRUE(result.empty()) << "static client must return empty from FetchModelsByIds";
}

// BYO synthesis: a locally cached model ID not in the snapshot is synthesized
// into a basic Local-provider entry by FetchAllModelInfosWithCachedModels.
TEST(StaticCatalogClientTest, BYOModel_SynthesizedWhenNotInSnapshot) {
  StderrLogger logger;
  StubEpDetector ep;
  auto client = MakeCatalogClient("static", "", ep, logger, "");
  auto result = FetchAllModelInfosWithCachedModels(*client, {"my-custom-model:0"}, logger);

  const ModelInfo* byo = nullptr;
  for (const auto& m : result) {
    if (m.model_id == "my-custom-model:0") {
      byo = &m;
    }
  }

  ASSERT_NE(byo, nullptr) << "BYO model entry should have been synthesized";
  EXPECT_EQ(byo->name, "my-custom-model");
  EXPECT_EQ(byo->uri, "local://my-custom-model");
  EXPECT_EQ(byo->string_properties.at(FOUNDRY_LOCAL_MODEL_PROP_MODEL_PROVIDER_STR), "Local");
}

// Verify that CatalogModelToModelInfo handles mixed-case device strings and bool tags
// case-insensitively. Lives in static_catalog_test.cc (always compiled) so we have
// coverage in public-repo builds where azure_catalog_test.cc is excluded.
TEST(CatalogModelToModelInfoTest, ParsesTagsCaseInsensitively) {
  auto build_model = [](const char* entity_id, const char* device, const char* tool_calling,
                        const char* reasoning) {
    CatalogLocalModel m;
    m.asset_id = std::string("azureml://registries/azureml/models/") + entity_id;
    m.entity_id = entity_id;

    CatalogTags tags;
    tags.alias = entity_id;
    if (tool_calling != nullptr) {
      tags.supports_tool_calling = tool_calling;
    }
    if (reasoning != nullptr) {
      tags.supports_reasoning = reasoning;
    }

    CatalogAnnotations ann;
    ann.tags = tags;
    m.annotations = ann;

    VariantMetadata vm;
    vm.device = device;
    vm.execution_provider = "CPUExecutionProvider";
    VariantInfo vi;
    vi.variant_metadata = vm;

    CatalogProperties props;
    props.name = entity_id;
    props.version = 1;
    props.variant_info = vi;
    m.properties = props;

    return m;
  };

  auto gpu = CatalogModelToModelInfo(build_model("m-gpu:1", "GpU", "TRUE", nullptr));
  ASSERT_TRUE(gpu.has_value());
  EXPECT_EQ(gpu->device_type, DeviceType::kGPU);
  EXPECT_EQ(gpu->int_properties.at(FOUNDRY_LOCAL_MODEL_PROP_SUPPORTS_TOOL_CALLING_INT), 1);

  auto npu = CatalogModelToModelInfo(build_model("m-npu:1", "NPU", "False", nullptr));
  ASSERT_TRUE(npu.has_value());
  EXPECT_EQ(npu->device_type, DeviceType::kNPU);
  EXPECT_EQ(npu->int_properties.at(FOUNDRY_LOCAL_MODEL_PROP_SUPPORTS_TOOL_CALLING_INT), 0);

  auto cpu = CatalogModelToModelInfo(build_model("m-cpu:1", "Cpu", nullptr, "tRuE"));
  ASSERT_TRUE(cpu.has_value());
  EXPECT_EQ(cpu->device_type, DeviceType::kCPU);
  EXPECT_EQ(cpu->int_properties.at(FOUNDRY_LOCAL_MODEL_PROP_SUPPORTS_REASONING_INT), 1);
}
