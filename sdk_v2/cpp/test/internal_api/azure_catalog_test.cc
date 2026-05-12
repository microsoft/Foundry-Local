// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Tests for the Azure catalog client infrastructure:
//   - Request JSON generation matches the known-good format
//   - Response JSON parsing works with realistic data
//   - Live integration test fetches models from ai.azure.com
//
// NOTE: Tests for the static catalog client live in static_catalog_test.cc
// so that they remain available when the live Azure catalog client source
// files have been removed (public-repo build).
//
#include "catalog/azure_catalog_client.h"
#include "catalog/azure_catalog_models.h"
#include "catalog/catalog_client.h"
#include "ep_detection/ep_detector.h"
#include "exception.h"
#include "logger.h"
#include "model_info.h"

#include <foundry_local/foundry_local_c.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using namespace fl;

// ========================================================================
// Test EP detector that returns all three devices (matching .http fixture)
// ========================================================================

class AllDevicesEpDetector : public IEpDetector {
 public:
  std::map<std::string, std::vector<std::string>> GetAvailableDevicesToEPs() const override {
    // Use PascalCase names matching ORT's GetEpDevices output.
    // BuildSearchFilters() lowers these for the catalog API.
    return {
        {"CPU", {"CPUExecutionProvider"}},
        {"GPU", {"CUDAExecutionProvider"}},
        {"NPU", {"QNNExecutionProvider"}},
    };
  }
};

// Single-device EP for tests where we only want one filter set
class CpuOnlyEpDetector : public IEpDetector {
 public:
  std::map<std::string, std::vector<std::string>> GetAvailableDevicesToEPs() const override {
    return {{"CPU", {"CPUExecutionProvider"}}};
  }
};

// ========================================================================
// Request format tests
// ========================================================================

// Verify the generated request JSON has the right structure.
// We compare against the known-good azure_catalog_model_fetch.http request.
TEST(AzureCatalogClientTest, RequestFormatMatchesKnownGood) {
  AllDevicesEpDetector ep;
  StderrLogger logger;
  // filter_override: "", "test" — matches the .http file's foundryLocal filter values
  AzureCatalogClient client("https://ai.azure.com/api/eastus/ux/v1.0", "''", ep, logger);

  // Capture the request body via the HttpPostFn hook.
  std::vector<std::string> captured_urls;
  std::vector<nlohmann::json> captured_bodies;

  client.SetHttpPost([&](const std::string& url, const std::string& body) -> std::string {
    captured_urls.push_back(url);
    captured_bodies.push_back(nlohmann::json::parse(body));
    // Return a valid empty response so pagination stops
    return R"({"indexEntitiesResponse":{"totalCount":0,"value":[],"nextSkip":0,"continuationToken":""}})";
  });

  client.FetchAllModels();

  // Our code creates one filter set per device (3 devices → 3 requests).
  ASSERT_EQ(captured_bodies.size(), 3u);

  for (const auto& url : captured_urls) {
    EXPECT_EQ(url, "https://ai.azure.com/api/eastus/ux/v1.0/entities/crossRegion");
  }

  // Verify the first request (cpu) matches the expected structure.
  // The .http file combines all devices; our C# pattern splits per device.
  // We verify the structure, field names, and operators match.
  const auto& cpu_req = captured_bodies[0];

  // Top-level structure
  ASSERT_TRUE(cpu_req.contains("resourceIds"));
  ASSERT_TRUE(cpu_req.contains("indexEntitiesRequest"));

  // resourceIds
  const auto& resources = cpu_req["resourceIds"];
  ASSERT_EQ(resources.size(), 1u);
  EXPECT_EQ(resources[0]["resourceId"], "azureml");
  EXPECT_EQ(resources[0]["entityContainerType"], "Registry");

  // indexEntitiesRequest
  const auto& idx_req = cpu_req["indexEntitiesRequest"];
  ASSERT_TRUE(idx_req.contains("filters"));
  EXPECT_EQ(idx_req["pageSize"], 50);

  // Verify filters match the known-good pattern
  const auto& filters = idx_req["filters"];
  ASSERT_EQ(filters.size(), 6u);

  // Filter 0: type=models
  EXPECT_EQ(filters[0]["field"], "type");
  EXPECT_EQ(filters[0]["operator"], "eq");
  EXPECT_EQ(filters[0]["values"], nlohmann::json({"models"}));

  // Filter 1: kind=Versioned
  EXPECT_EQ(filters[1]["field"], "kind");
  EXPECT_EQ(filters[1]["operator"], "eq");
  EXPECT_EQ(filters[1]["values"], nlohmann::json({"Versioned"}));

  // Filter 2: labels=latest
  EXPECT_EQ(filters[2]["field"], "labels");
  EXPECT_EQ(filters[2]["operator"], "eq");
  EXPECT_EQ(filters[2]["values"], nlohmann::json({"latest"}));

  // Filter 3: foundryLocal filter — matches "", "test" from the .http file
  EXPECT_EQ(filters[3]["field"], "annotations/tags/foundryLocal");
  EXPECT_EQ(filters[3]["operator"], "eq");
  EXPECT_EQ(filters[3]["values"], nlohmann::json({"", "test"}));

  // Filter 4: device (per-device split — this is "cpu" for the first request)
  EXPECT_EQ(filters[4]["field"], "properties/variantInfo/variantMetadata/device");
  EXPECT_EQ(filters[4]["operator"], "eq");
  EXPECT_EQ(filters[4]["values"], nlohmann::json({"cpu"}));

  // Filter 5: executionProvider
  EXPECT_EQ(filters[5]["field"], "properties/variantInfo/variantMetadata/executionProvider");
  EXPECT_EQ(filters[5]["operator"], "eq");
}

// Verify page size default is 50 (matching C#) and can be set.
TEST(AzureCatalogClientTest, DefaultPageSizeIs50) {
  AllDevicesEpDetector ep;
  StderrLogger logger;
  AzureCatalogClient client("https://ai.azure.com/api/eastus/ux/v1.0", "", ep, logger);

  nlohmann::json captured;
  client.SetHttpPost([&](const std::string&, const std::string& body) -> std::string {
    captured = nlohmann::json::parse(body);
    return R"({"indexEntitiesResponse":{"totalCount":0,"value":[],"nextSkip":0,"continuationToken":""}})";
  });

  client.FetchAllModels();
  EXPECT_EQ(captured["indexEntitiesRequest"]["pageSize"], 50);
}

// ========================================================================
// Response parsing tests
// ========================================================================

TEST(AzureCatalogClientTest, ParsesModelResponseCorrectly) {
  CpuOnlyEpDetector ep;
  StderrLogger logger;
  AzureCatalogClient client("https://test.com", "", ep, logger);

  // Realistic single-model response matching the Azure catalog schema
  const char* mock_response = R"({
    "indexEntitiesResponse": {
      "totalCount": 1,
      "value": [
        {
          "assetId": "azureml://registries/azureml/models/Phi-4-mini-instruct-generic-cpu/versions/2",
          "entityId": "Phi-4-mini-instruct-generic-cpu:2",
          "annotations": {
            "tags": {
              "alias": "Phi-4-mini-instruct",
              "foundryLocal": "",
              "task": "chat-completion",
              "license": "MIT",
              "licenseDescription": "MIT License",
              "supportsToolCalling": "true",
              "promptTemplate": "{\"system\":\"<|system|>\\n{Content}<|end|>\",\"user\":\"<|user|>\\n{Content}<|end|>\",\"assistant\":\"<|assistant|>\\n{Content}<|end|>\",\"prompt\":\"<|user|>\\n{Content}<|end|>\\n<|assistant|>\"}"
            },
            "systemCatalogData": {
              "publisher": "Microsoft",
              "displayName": "Phi-4 Mini Instruct",
              "maxOutputTokens": 4096
            }
          },
          "properties": {
            "name": "Phi-4-mini-instruct-generic-cpu",
            "version": 2,
            "variantInfo": {
              "parents": [{"assetId": "azureml://registries/azureml/models/Phi-4-mini-instruct/versions/3"}],
              "variantMetadata": {
                "modelType": "ONNX",
                "device": "cpu",
                "executionProvider": "CPUExecutionProvider",
                "fileSizeBytes": 4294967296
              }
            },
            "minFLVersion": "0.3.0"
          }
        }
      ],
      "nextSkip": 0,
      "continuationToken": ""
    }
  })";

  client.SetHttpPost([&](const std::string&, const std::string&) -> std::string {
    return mock_response;
  });

  auto model_infos = client.FetchAllModelInfos();
  ASSERT_EQ(model_infos.size(), 1u);

  const auto& info = model_infos[0];
  EXPECT_EQ(info.model_id, "Phi-4-mini-instruct-generic-cpu:2");
  EXPECT_EQ(info.name, "Phi-4-mini-instruct-generic-cpu");
  EXPECT_EQ(info.alias, "Phi-4-mini-instruct");
  EXPECT_EQ(info.version, 2);
  EXPECT_EQ(info.uri, "azureml://registries/azureml/models/Phi-4-mini-instruct-generic-cpu/versions/2");
  EXPECT_EQ(info.device_type, DeviceType::kCPU);
  EXPECT_EQ(info.execution_provider, "CPUExecutionProvider");

  // Prompt templates should be parsed from the JSON string
  ASSERT_FALSE(info.prompt_templates.empty());
  EXPECT_TRUE(info.prompt_templates.count("prompt") > 0);
  EXPECT_TRUE(info.prompt_templates.count("system") > 0);
  EXPECT_TRUE(info.prompt_templates.count("user") > 0);
  EXPECT_TRUE(info.prompt_templates.count("assistant") > 0);

  // Metadata properties
  EXPECT_EQ(info.string_properties.at(FOUNDRY_LOCAL_MODEL_PROP_TASK_STR), "chat-completion");
  EXPECT_EQ(info.string_properties.at(FOUNDRY_LOCAL_MODEL_PROP_LICENSE_STR), "MIT");
  EXPECT_EQ(info.string_properties.at(FOUNDRY_LOCAL_MODEL_PROP_PUBLISHER_STR), "Microsoft");
  EXPECT_EQ(info.string_properties.at(FOUNDRY_LOCAL_MODEL_PROP_DISPLAY_NAME_STR), "Phi-4 Mini Instruct");
  EXPECT_EQ(info.int_properties.at(FOUNDRY_LOCAL_MODEL_PROP_SUPPORTS_TOOL_CALLING_INT), 1);
  EXPECT_EQ(info.string_properties.at(FOUNDRY_LOCAL_MODEL_PROP_MIN_FL_VERSION_STR), "0.3.0");
  EXPECT_EQ(info.string_properties.at(FOUNDRY_LOCAL_MODEL_PROP_MODEL_PROVIDER_STR), "AzureFoundry");
  EXPECT_EQ(info.int_properties.at(FOUNDRY_LOCAL_MODEL_PROP_MAX_OUTPUT_TOKENS_INT), 4096);
  EXPECT_EQ(info.int_properties.at(FOUNDRY_LOCAL_MODEL_PROP_FILESIZE_MB_INT), 4096);  // 4GB → 4096 MB
}

// Verify that invalid models (missing required fields) are filtered out
TEST(AzureCatalogClientTest, SkipsInvalidModels) {
  CpuOnlyEpDetector ep;
  StderrLogger logger;
  AzureCatalogClient client("https://test.com", "", ep, logger);

  // Response with one valid model and one missing assetId
  const char* mock_response = R"({
    "indexEntitiesResponse": {
      "totalCount": 2,
      "value": [
        {
          "entityId": "no-asset-id",
          "properties": { "name": "bad-model", "version": 1, "variantInfo": { "parents": [] } }
        },
        {
          "assetId": "azureml://registries/azureml/models/good-model/versions/1",
          "entityId": "good-model:1",
          "annotations": { "tags": { "alias": "good" } },
          "properties": {
            "name": "good-model",
            "version": 1,
            "variantInfo": {
              "parents": [],
              "variantMetadata": { "device": "cpu", "executionProvider": "CPUExecutionProvider" }
            }
          }
        }
      ],
      "nextSkip": 0,
      "continuationToken": ""
    }
  })";

  client.SetHttpPost([&](const std::string&, const std::string&) -> std::string {
    return mock_response;
  });

  auto infos = client.FetchAllModelInfos();
  ASSERT_EQ(infos.size(), 1u);
  EXPECT_EQ(infos[0].alias, "good");
}

// Verify pagination follows nextSkip and continuationToken
TEST(AzureCatalogClientTest, FollowsPagination) {
  AllDevicesEpDetector ep;
  // Use a single device so we only test one filter set
  class SingleDeviceEp : public IEpDetector {
   public:
    std::map<std::string, std::vector<std::string>> GetAvailableDevicesToEPs() const override {
      return {{"CPU", {"CPUExecutionProvider"}}};
    }
  } single_ep;

  StderrLogger logger;
  AzureCatalogClient client("https://test.com", "", single_ep, logger);

  int call_count = 0;
  client.SetHttpPost([&](const std::string&, const std::string& body) -> std::string {
    call_count++;
    auto req = nlohmann::json::parse(body);

    if (call_count == 1) {
      // First page — return nextSkip to trigger page 2
      return R"({
        "indexEntitiesResponse": {
          "totalCount": 2,
          "value": [{
            "assetId": "azureml://m/model-a/versions/1",
            "entityId": "model-a:1",
            "annotations": {"tags": {"alias": "a"}},
            "properties": {"name": "model-a", "version": 1, "variantInfo": {"parents": [], "variantMetadata": {"device": "cpu"}}}
          }],
          "nextSkip": 1,
          "continuationToken": "token123"
        }
      })";
    } else {
      // Second page — no more
      // Verify skip/token were passed
      EXPECT_EQ(req["indexEntitiesRequest"]["skip"], 1);
      EXPECT_EQ(req["indexEntitiesRequest"]["continuationToken"], "token123");
      return R"({
        "indexEntitiesResponse": {
          "totalCount": 2,
          "value": [{
            "assetId": "azureml://m/model-b/versions/1",
            "entityId": "model-b:1",
            "annotations": {"tags": {"alias": "b"}},
            "properties": {"name": "model-b", "version": 1, "variantInfo": {"parents": [], "variantMetadata": {"device": "cpu"}}}
          }],
          "nextSkip": 0,
          "continuationToken": ""
        }
      })";
    }
  });

  auto models = client.FetchAllModels();
  EXPECT_EQ(call_count, 2);
  EXPECT_EQ(models.size(), 2u);
}

// ========================================================================
// Live integration test — fetches real models from ai.azure.com
// Disabled by default. Run with: --gtest_also_run_disabled_tests
// ========================================================================

TEST(AzureCatalogClientTest, LiveFetchModelsFromAzure)
{
  AllDevicesEpDetector ep;
  StderrLogger logger;
  AzureCatalogClient client("https://ai.azure.com/api/eastus/ux/v1.0", "''", ep, logger);

  // Use the real HTTP client (WinHTTP on Windows)
  auto model_infos = client.FetchAllModelInfos();

  // We should get at least some models from the public catalog
  EXPECT_GT(model_infos.size(), 0u)
      << "Expected at least one model from Azure Foundry catalog";

  // Every model should have the basic required fields populated
  for (const auto& info : model_infos) {
    EXPECT_FALSE(info.model_id.empty()) << "model_id should not be empty";
    EXPECT_FALSE(info.name.empty()) << "name should not be empty";
    EXPECT_FALSE(info.alias.empty()) << "alias should not be empty for: " << info.model_id;
    EXPECT_FALSE(info.uri.empty()) << "uri should not be empty for: " << info.model_id;
  }

  // Print summary for manual verification
  std::cout << "\n=== Live Azure Catalog Results ===\n";
  std::cout << "Total models: " << model_infos.size() << "\n";
  for (const auto& info : model_infos) {
    std::cout << "  " << info.alias
              << " (v" << info.version << ")"
              << " [" << info.execution_provider << "]"
              << "\n";
  }
  std::cout << "=================================\n";
}

// ========================================================================
// FetchModelsByIds filter construction tests
// ========================================================================

TEST(AzureCatalogClientTest, BuildModelIdFiltersProducesCorrectStructure) {
  CpuOnlyEpDetector ep;
  StderrLogger logger;
  AzureCatalogClient client("https://test.com", "", ep, logger);

  nlohmann::json captured;
  client.SetHttpPost([&](const std::string&, const std::string& body) -> std::string {
    captured = nlohmann::json::parse(body);
    return R"({"indexEntitiesResponse":{"totalCount":0,"value":[],"nextSkip":0,"continuationToken":""}})";
  });

  client.FetchModelsByIds({"phi-4-mini:3", "llama-3:1"});

  // Should have made exactly one HTTP call (single filter set, no pagination).
  ASSERT_FALSE(captured.is_null());

  const auto& filters = captured["indexEntitiesRequest"]["filters"];
  ASSERT_EQ(filters.size(), 4u);

  // Filter 0: type=models
  EXPECT_EQ(filters[0]["field"], "type");
  EXPECT_EQ(filters[0]["values"], nlohmann::json({"models"}));

  // Filter 1: kind=Versioned
  EXPECT_EQ(filters[1]["field"], "kind");
  EXPECT_EQ(filters[1]["values"], nlohmann::json({"Versioned"}));

  // Filter 2: foundryLocal tag (no labels=latest!)
  EXPECT_EQ(filters[2]["field"], "annotations/tags/foundryLocal");

  // Filter 3: properties/id with the requested model IDs
  EXPECT_EQ(filters[3]["field"], "properties/id");
  EXPECT_EQ(filters[3]["values"], nlohmann::json({"phi-4-mini:3", "llama-3:1"}));

  // Verify NO labels filter and NO device/EP filters exist.
  for (const auto& f : filters) {
    EXPECT_NE(f["field"], "labels");
    EXPECT_NE(f["field"], "properties/variantInfo/variantMetadata/device");
    EXPECT_NE(f["field"], "properties/variantInfo/variantMetadata/executionProvider");
  }
}

TEST(AzureCatalogClientTest, FetchModelsByIdsEmptyReturnsEmptyNoHttp) {
  CpuOnlyEpDetector ep;
  StderrLogger logger;
  AzureCatalogClient client("https://test.com", "", ep, logger);

  bool http_called = false;
  client.SetHttpPost([&](const std::string&, const std::string&) -> std::string {
    http_called = true;
    return R"({"indexEntitiesResponse":{"totalCount":0,"value":[],"nextSkip":0,"continuationToken":""}})";
  });

  auto result = client.FetchModelsByIds({});
  EXPECT_TRUE(result.empty());
  EXPECT_FALSE(http_called);
}

// ========================================================================
// FetchAllModelInfosWithCachedModels tests
// ========================================================================

// Helper: builds a minimal valid model response for a given model name.
static std::string MakeMockCatalogResponse(
    const std::vector<std::pair<std::string, int>>& models) {
  nlohmann::json value_array = nlohmann::json::array();

  for (const auto& [name, version] : models) {
    std::string entity_id = name + ":" + std::to_string(version);

    nlohmann::json entry;
    entry["assetId"] = "azureml://registries/azureml/models/" + name + "/versions/" +
                       std::to_string(version);
    entry["entityId"] = entity_id;
    entry["annotations"]["tags"]["alias"] = name;
    entry["properties"]["name"] = name;
    entry["properties"]["version"] = version;
    entry["properties"]["variantInfo"]["parents"] = nlohmann::json::array();
    entry["properties"]["variantInfo"]["variantMetadata"]["device"] = "cpu";
    entry["properties"]["variantInfo"]["variantMetadata"]["executionProvider"] =
        "CPUExecutionProvider";
    value_array.push_back(entry);
  }

  nlohmann::json response;
  response["indexEntitiesResponse"]["totalCount"] = static_cast<int>(models.size());
  response["indexEntitiesResponse"]["value"] = value_array;
  response["indexEntitiesResponse"]["nextSkip"] = 0;
  response["indexEntitiesResponse"]["continuationToken"] = "";
  return response.dump();
}

TEST(AzureCatalogClientTest, WithCachedModels_NoCachedIds_BehavesLikeRegularFetch) {
  CpuOnlyEpDetector ep;
  StderrLogger logger;
  AzureCatalogClient client("https://test.com", "", ep, logger);

  int http_call_count = 0;
  client.SetHttpPost([&](const std::string&, const std::string&) -> std::string {
    http_call_count++;
    return MakeMockCatalogResponse({{"phi-4-mini", 3}});
  });

  auto result = FetchAllModelInfosWithCachedModels(client, {}, logger);

  // Only the primary FetchAllModelInfos call — no extra fetch for cached models.
  EXPECT_EQ(http_call_count, 1);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0].model_id, "phi-4-mini:3");
}

TEST(AzureCatalogClientTest, WithCachedModels_AlreadyInCatalog_NoExtraFetch) {
  CpuOnlyEpDetector ep;
  StderrLogger logger;
  AzureCatalogClient client("https://test.com", "", ep, logger);

  int http_call_count = 0;
  client.SetHttpPost([&](const std::string&, const std::string&) -> std::string {
    http_call_count++;
    return MakeMockCatalogResponse({{"phi-4-mini", 3}});
  });

  // The cached ID matches what's already in the catalog — no extra fetch needed.
  auto result = FetchAllModelInfosWithCachedModels(client, {"phi-4-mini:3"}, logger);

  EXPECT_EQ(http_call_count, 1);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0].model_id, "phi-4-mini:3");
}

TEST(AzureCatalogClientTest, WithCachedModels_UnresolvedId_TriggersSecondFetch) {
  CpuOnlyEpDetector ep;
  StderrLogger logger;
  AzureCatalogClient client("https://test.com", "", ep, logger);

  int http_call_count = 0;
  client.SetHttpPost([&](const std::string&, const std::string& body) -> std::string {
    http_call_count++;

    if (http_call_count == 1) {
      // Primary catalog fetch — returns phi-4-mini only.
      return MakeMockCatalogResponse({{"phi-4-mini", 3}});
    } else {
      // Second fetch — looking up the unresolved model by ID.
      auto req = nlohmann::json::parse(body);
      const auto& filters = req["indexEntitiesRequest"]["filters"];

      // Verify the second call uses properties/id filter.
      bool has_id_filter = false;
      for (const auto& f : filters) {
        if (f["field"] == "properties/id") {
          has_id_filter = true;
          EXPECT_EQ(f["values"], nlohmann::json({"old-model:1"}));
        }
      }

      EXPECT_TRUE(has_id_filter);

      return MakeMockCatalogResponse({{"old-model", 1}});
    }
  });

  auto result = FetchAllModelInfosWithCachedModels(client, {"old-model:1"}, logger);

  EXPECT_EQ(http_call_count, 2);
  ASSERT_EQ(result.size(), 2u);

  // Verify both models are present.
  bool found_phi = false;
  bool found_old = false;
  for (const auto& info : result) {
    if (info.model_id == "phi-4-mini:3") {
      found_phi = true;
    }

    if (info.model_id == "old-model:1") {
      found_old = true;
    }
  }

  EXPECT_TRUE(found_phi);
  EXPECT_TRUE(found_old);
}

TEST(AzureCatalogClientTest, WithCachedModels_FullyUnresolved_CreatesBYOEntry) {
  CpuOnlyEpDetector ep;
  StderrLogger logger;
  AzureCatalogClient client("https://test.com", "", ep, logger);

  int http_call_count = 0;
  client.SetHttpPost([&](const std::string&, const std::string&) -> std::string {
    http_call_count++;

    if (http_call_count == 1) {
      // Primary catalog — returns nothing matching.
      return MakeMockCatalogResponse({{"phi-4-mini", 3}});
    } else {
      // FetchModelsByIds — also returns nothing for the custom model.
      return R"({"indexEntitiesResponse":{"totalCount":0,"value":[],"nextSkip":0,"continuationToken":""}})";
    }
  });

  auto result = FetchAllModelInfosWithCachedModels(client, {"custom-model:0"}, logger);

  EXPECT_EQ(http_call_count, 2);

  // Find the BYO entry.
  const ModelInfo* byo = nullptr;
  for (const auto& info : result) {
    if (info.model_id == "custom-model:0") {
      byo = &info;
    }
  }

  ASSERT_NE(byo, nullptr);
  EXPECT_EQ(byo->name, "custom-model");
  EXPECT_EQ(byo->alias, "custom-model");
  EXPECT_EQ(byo->uri, "local://custom-model");
  EXPECT_EQ(byo->version, 0);
  EXPECT_EQ(byo->string_properties.at(FOUNDRY_LOCAL_MODEL_PROP_MODEL_PROVIDER_STR), "Local");
  EXPECT_EQ(byo->string_properties.at(FOUNDRY_LOCAL_MODEL_PROP_MODEL_TYPE_STR), "ONNX");
}

// ========================================================================
// Reasoning field parsing tests
// ========================================================================

TEST(AzureCatalogClientTest, ParsesReasoningFieldsCorrectly) {
  CpuOnlyEpDetector ep;
  StderrLogger logger;
  AzureCatalogClient client("https://test.com", "", ep, logger);

  const char* mock_response = R"({
    "indexEntitiesResponse": {
      "totalCount": 1,
      "value": [{
        "assetId": "azureml://registries/azureml/models/Phi-4-mini-reasoning-generic-cpu/versions/1",
        "entityId": "Phi-4-mini-reasoning-generic-cpu:1",
        "annotations": {
          "tags": {
            "alias": "Phi-4-mini-reasoning",
            "foundryLocal": "",
            "task": "chat-completion",
            "supportsToolCalling": "true",
            "supportsReasoning": "true",
            "reasoningStart": "<think>",
            "reasoningEnd": "</think>"
          },
          "systemCatalogData": {
            "publisher": "Microsoft",
            "displayName": "Phi-4 Mini Reasoning"
          }
        },
        "properties": {
          "name": "Phi-4-mini-reasoning-generic-cpu",
          "version": 1,
          "variantInfo": {
            "parents": [{"assetId": "azureml://registries/azureml/models/Phi-4-mini-reasoning/versions/1"}],
            "variantMetadata": {
              "modelType": "ONNX",
              "device": "cpu",
              "executionProvider": "CPUExecutionProvider"
            }
          }
        }
      }],
      "nextSkip": 0,
      "continuationToken": ""
    }
  })";

  client.SetHttpPost([&](const std::string&, const std::string&) -> std::string {
    return mock_response;
  });

  auto model_infos = client.FetchAllModelInfos();
  ASSERT_EQ(model_infos.size(), 1u);

  const auto& info = model_infos[0];

  // Core identity
  EXPECT_EQ(info.model_id, "Phi-4-mini-reasoning-generic-cpu:1");
  EXPECT_EQ(info.alias, "Phi-4-mini-reasoning");

  // Reasoning fields
  EXPECT_EQ(info.int_properties.at(FOUNDRY_LOCAL_MODEL_PROP_SUPPORTS_REASONING_INT), 1);
  EXPECT_EQ(info.string_properties.at(FOUNDRY_LOCAL_MODEL_PROP_REASONING_START_STR), "<think>");
  EXPECT_EQ(info.string_properties.at(FOUNDRY_LOCAL_MODEL_PROP_REASONING_END_STR), "</think>");

  // Tool calling
  EXPECT_EQ(info.int_properties.at(FOUNDRY_LOCAL_MODEL_PROP_SUPPORTS_TOOL_CALLING_INT), 1);
}
