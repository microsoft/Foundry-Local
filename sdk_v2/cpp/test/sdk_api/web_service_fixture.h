// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
// Helper fixture for web-service integration tests.
//
// The HTTP service itself is started ONCE for the whole test process by
// SharedTestEnv (it's a stateless dispatcher above the Manager and doesn't
// care which models are resident). This fixture just exposes a configured
// httplib client and the convenience accessors that suites typically need.
//
// Suites declare their own model needs:
//
//   class MySuite : public WebServiceFixture {
//     static void SetUpTestSuite() {
//       SharedTestEnv::Get().AcquireModels({SharedTestEnv::Modality::Chat});
//     }
//     void SetUp() override {
//       if (!SharedTestEnv::Get().chat_model()) GTEST_SKIP() << "...";
//     }
//   };
#pragma once

#include "model_fixture.h"

class WebServiceFixture : public ::testing::Test {
 protected:
  // Default read timeout. Override per-test with client.set_read_timeout()
  // for slow paths (e.g. CPU vision inference).
  httplib::Client MakeClient(int read_timeout_seconds = 60) {
    httplib::Client client(SharedTestEnv::Get().web_service_url());
    client.set_connection_timeout(10, 0);
    client.set_read_timeout(read_timeout_seconds, 0);
    return client;
  }

  // Convenience accessors. Tests that need a specific model still must
  // AcquireModels in their SetUpTestSuite — these just save typing.
  static foundry_local::Manager& manager() { return *SharedTestEnv::Get().manager(); }
  static foundry_local::ICatalog& catalog() { return SharedTestEnv::Get().catalog(); }
};

// Web-service fixture that ALSO acquires the chat model. Used by
// chat-completions, /v1/responses, and model-endpoint tests that route
// requests through a chat model.
class WebServiceIntegrationTest : public WebServiceFixture {
 protected:
  static void SetUpTestSuite() {
    SharedTestEnv::Get().AcquireModels({SharedTestEnv::Modality::Chat});
  }

  void SetUp() override {
    if (!SharedTestEnv::Get().chat_model()) {
      GTEST_SKIP() << "No chat-completion model available";
    }
  }

  static foundry_local::IModel& chat_model() { return *SharedTestEnv::Get().chat_model(); }
  static const std::string& model_id() { return SharedTestEnv::Get().chat_model_id(); }
  static const std::string& model_alias() { return SharedTestEnv::Get().chat_model_alias(); }
};
