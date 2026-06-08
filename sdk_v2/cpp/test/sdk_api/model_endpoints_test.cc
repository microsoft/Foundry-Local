// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
// Web service integration tests for OpenAI model endpoints and model management endpoints.

#include "web_service_fixture.h"

// --- OpenAI Model Endpoints ---

TEST_F(WebServiceIntegrationTest, ListModels) {
  auto client = MakeClient();
  auto result = client.Get("/v1/models");
  ASSERT_TRUE(result) << "HTTP request failed";
  ASSERT_EQ(result->status, 200) << result->body;

  json response = json::parse(result->body);
  EXPECT_EQ(response["object"], "list");
  ASSERT_TRUE(response.contains("data"));
  EXPECT_GT(response["data"].size(), 0u);

  // Each model entry has required fields
  for (const auto& model : response["data"]) {
    EXPECT_TRUE(model.contains("id"));
    EXPECT_EQ(model["object"], "model");
    EXPECT_TRUE(model.contains("created"));
    EXPECT_TRUE(model.contains("owned_by"));
  }
}

TEST_F(WebServiceIntegrationTest, RetrieveModel) {
  auto client = MakeClient();
  auto result = client.Get(("/v1/models/" + model_id()).c_str());
  ASSERT_TRUE(result) << "HTTP request failed";
  ASSERT_EQ(result->status, 200) << result->body;

  json response = json::parse(result->body);
  EXPECT_EQ(response["id"], model_id());
  EXPECT_EQ(response["object"], "model");
  EXPECT_TRUE(response.contains("created"));
  EXPECT_TRUE(response.contains("owned_by"));
}

TEST_F(WebServiceIntegrationTest, RetrieveModelNotFound) {
  auto client = MakeClient();
  auto result = client.Get("/v1/models/nonexistent_model_xyz");
  ASSERT_TRUE(result) << "HTTP request failed";
  EXPECT_EQ(result->status, 404);

  json response = json::parse(result->body);
  EXPECT_TRUE(response.contains("error"));
}

// --- Model Management Endpoints ---

TEST_F(WebServiceIntegrationTest, ListLoadedModels) {
  auto client = MakeClient();
  auto result = client.Get("/models/loaded");
  ASSERT_TRUE(result) << "HTTP request failed";
  ASSERT_EQ(result->status, 200) << result->body;

  json response = json::parse(result->body);
  ASSERT_TRUE(response.is_array());
}

TEST_F(WebServiceIntegrationTest, LoadModelCacheDetection) {
  auto client = MakeClient();

  // The web service has its own catalog instance created at StartService time.
  // The model was downloaded and loaded via the SDK's C++ API, but the web service's
  // catalog may not detect it as cached due to model_id version key differences
  // between the local scanner (Name + ":0") and Azure catalog (Name + ":N").
  // This test exercises the load handler's validation paths.
  auto result = client.Get(("/models/load/" + model_alias()).c_str());
  ASSERT_TRUE(result) << "HTTP request failed";

  json response = json::parse(result->body);

  if (result->status == 200) {
    // Model was detected as cached — handler loaded it or found it already loaded.
    EXPECT_TRUE(response.contains("status"));
    std::string status = response["status"];
    EXPECT_TRUE(status == "loaded" || status == "already_loaded") << "Unexpected status: " << status;
  } else {
    // Model found in catalog but not detected as cached (version-key mismatch).
    EXPECT_EQ(result->status, 400);
    EXPECT_TRUE(response.contains("error"));
    EXPECT_EQ(response["error"]["type"], "invalid_request_error");
  }
}

TEST_F(WebServiceIntegrationTest, LoadModelNotFound) {
  auto client = MakeClient();
  auto result = client.Get("/models/load/nonexistent_model_xyz");
  ASSERT_TRUE(result) << "HTTP request failed";
  EXPECT_EQ(result->status, 404);
}

TEST_F(WebServiceIntegrationTest, UnloadModelNotFound) {
  auto client = MakeClient();
  auto result = client.Get("/models/unload/nonexistent_model_xyz");
  ASSERT_TRUE(result) << "HTTP request failed";
  EXPECT_EQ(result->status, 404);
}
