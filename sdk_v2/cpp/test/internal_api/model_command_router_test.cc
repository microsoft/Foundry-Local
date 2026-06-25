// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Tests for fl::ModelCommandRouter — the façade that owns the local-vs-external
// decision for the three model-management commands (list-loaded / load / unload).
//
// Coverage map:
//   * Local branch  — wraps a real ModelLoadManager with external_service_url unset.
//                     Exercised with the cheap, model-free idempotent/empty cases.
//   * External branch — pointed at a tiny in-process httplib stub server returning
//                     canned bodies. Covers base-URL trailing-slash trimming, the
//                     JSON-array parse (empty/whitespace → {}, ["a","b"] → {a,b}),
//                     url-encoding of the id path segment, membership-based IsLoaded,
//                     and FOUNDRY_LOCAL_ERROR_NETWORK on non-2xx / transport failure.
//   * Real web service round-trip (guarded by FOUNDRY_LOCAL_HAS_WEB_SERVICE) — points
//                     an external-mode router at a real Foundry Local WebService and
//                     verifies load → list → IsLoaded → unload, mirroring v1's
//                     TestModelLoadManagerExternalService. Also asserts the
//                     BaseModelCatalog::GetLoadedModels() external-mode regression fix.

#include "model_command_router.h"

#include "catalog/base_model_catalog.h"
#include "ep_detection/ep_detector.h"
#include "exception.h"
#include "inferencing/execution_provider.h"
#include "inferencing/model_load_manager.h"
#include "internal_api/test_helpers.h"
#include "logger.h"
#include "model.h"
#include "model_info.h"

#include <foundry_local/foundry_local_c.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <httplib.h>

using namespace fl;

namespace {

// ========================================================================
// Local-branch tests — no server, no model. These exercise the cheap
// idempotent/empty paths that the design promises without loading anything.
// ========================================================================

class RouterLocalTest : public ::testing::Test {
 protected:
  test::CpuOnlyEpDetector ep_;
  StderrLogger logger_;
  ModelLoadManager load_manager_{ep_, logger_};
  ModelCommandRouter router_{/*external_service_url=*/std::nullopt, load_manager_, "test", logger_};
};

TEST_F(RouterLocalTest, ListLoadedModelIds_EmptyWhenNothingLoaded) {
  EXPECT_TRUE(router_.ListLoadedModelIds().empty());
}

TEST_F(RouterLocalTest, IsLoaded_FalseWhenNotLoaded) {
  EXPECT_FALSE(router_.IsLoaded("never-loaded:1"));
}

TEST_F(RouterLocalTest, Unload_UnloadedId_IsIdempotentNoThrow) {
  // UnloadModel returns false for an unknown id; the router must swallow that and not throw.
  EXPECT_NO_THROW(router_.Unload("never-loaded:1"));
}

// ========================================================================
// External-branch tests — a tiny in-process httplib stub server returns
// canned bodies so the HTTP path is exercised deterministically without a
// real model or the full web service.
// ========================================================================

// Minimal canned-response HTTP server. Records every requested path so tests can
// assert URL shape (trailing-slash trimming, url-encoded id segment).
class StubServer {
 public:
  StubServer() {
    server_.Get("/.*", [this](const httplib::Request& req, httplib::Response& res) {
      {
        std::lock_guard<std::mutex> lock(mutex_);
        // Record the raw, undecoded request-target (not req.path, which httplib percent-decodes)
        // so url-encoding assertions can see the wire bytes the router actually sent.
        requested_paths_.push_back(req.target);
      }

      res.status = response_status_;
      res.set_content(response_body_, "application/json");
    });

    // bind_to_any_port() binds to an OS-assigned ephemeral port and returns it. (bind_to_port()
    // returns a bool, not the port — using it here would silently yield port 1.)
    port_ = server_.bind_to_any_port("127.0.0.1");

    listener_ = std::thread([this]() { server_.listen_after_bind(); });

    // The socket is already in listen() state after bind, but wait until the accept loop is
    // actually spun up so the very first request can't race the thread start.
    while (!server_.is_running()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  ~StubServer() {
    server_.stop();
    if (listener_.joinable()) {
      listener_.join();
    }
  }

  StubServer(const StubServer&) = delete;
  StubServer& operator=(const StubServer&) = delete;

  std::string BaseUrl() const { return "http://127.0.0.1:" + std::to_string(port_); }

  void SetResponse(int status, std::string body) {
    response_status_ = status;
    response_body_ = std::move(body);
  }

  std::vector<std::string> RequestedPaths() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return requested_paths_;
  }

 private:
  httplib::Server server_;
  int port_ = 0;
  std::thread listener_;

  std::atomic<int> response_status_{200};
  std::string response_body_{"[]"};

  mutable std::mutex mutex_;
  std::vector<std::string> requested_paths_;
};

class RouterExternalTest : public ::testing::Test {
 protected:
  StubServer server_;
  test::CpuOnlyEpDetector ep_;
  StderrLogger logger_;
  ModelLoadManager load_manager_{ep_, logger_};
};

TEST_F(RouterExternalTest, ListLoadedModelIds_ParsesJsonArray) {
  server_.SetResponse(200, R"(["alpha:1","beta:2"])");
  ModelCommandRouter router(server_.BaseUrl(), load_manager_, "test", logger_);

  auto ids = router.ListLoadedModelIds();

  ASSERT_EQ(ids.size(), 2u);
  EXPECT_EQ(ids[0], "alpha:1");
  EXPECT_EQ(ids[1], "beta:2");

  auto paths = server_.RequestedPaths();
  ASSERT_EQ(paths.size(), 1u);
  EXPECT_EQ(paths[0], "/models/loaded");
}

TEST_F(RouterExternalTest, ListLoadedModelIds_EmptyBodyYieldsEmptyVector) {
  server_.SetResponse(200, "");
  ModelCommandRouter router(server_.BaseUrl(), load_manager_, "test", logger_);

  EXPECT_TRUE(router.ListLoadedModelIds().empty());
}

TEST_F(RouterExternalTest, ListLoadedModelIds_WhitespaceBodyYieldsEmptyVector) {
  server_.SetResponse(200, "  \r\n\t ");
  ModelCommandRouter router(server_.BaseUrl(), load_manager_, "test", logger_);

  EXPECT_TRUE(router.ListLoadedModelIds().empty());
}

TEST_F(RouterExternalTest, ListLoadedModelIds_EmptyJsonArrayYieldsEmptyVector) {
  server_.SetResponse(200, "[]");
  ModelCommandRouter router(server_.BaseUrl(), load_manager_, "test", logger_);

  EXPECT_TRUE(router.ListLoadedModelIds().empty());
}

TEST_F(RouterExternalTest, BaseUrl_TrailingSlashIsTrimmed) {
  server_.SetResponse(200, "[]");
  // The configured URL carries a trailing '/'; the appended "/models/loaded" must not double up.
  ModelCommandRouter router(server_.BaseUrl() + "/", load_manager_, "test", logger_);

  router.ListLoadedModelIds();

  auto paths = server_.RequestedPaths();
  ASSERT_EQ(paths.size(), 1u);
  EXPECT_EQ(paths[0], "/models/loaded") << "Expected no doubled slash from the trailing-'/' base URL";
}

TEST_F(RouterExternalTest, IsLoaded_TrueWhenIdPresentInLoadedSet) {
  server_.SetResponse(200, R"(["phi:1","qwen:3"])");
  ModelCommandRouter router(server_.BaseUrl(), load_manager_, "test", logger_);

  EXPECT_TRUE(router.IsLoaded("qwen:3"));
}

TEST_F(RouterExternalTest, IsLoaded_FalseWhenIdAbsentFromLoadedSet) {
  server_.SetResponse(200, R"(["phi:1","qwen:3"])");
  ModelCommandRouter router(server_.BaseUrl(), load_manager_, "test", logger_);

  EXPECT_FALSE(router.IsLoaded("missing:9"));
}

TEST_F(RouterExternalTest, Load_UrlEncodesIdInPathSegment) {
  server_.SetResponse(200, R"({"status":"loaded"})");
  ModelCommandRouter router(server_.BaseUrl(), load_manager_, "test", logger_);

  // ':' is reserved and must be percent-encoded to %3A in the path segment.
  router.Load("phi-4:1", "/ignored/in/external/mode", ExecutionProvider::kCPU);

  auto paths = server_.RequestedPaths();
  ASSERT_EQ(paths.size(), 1u);
  EXPECT_EQ(paths[0], "/models/load/phi-4%3A1");
}

TEST_F(RouterExternalTest, Unload_UrlEncodesIdInPathSegment) {
  server_.SetResponse(200, R"({"status":"unloaded"})");
  ModelCommandRouter router(server_.BaseUrl(), load_manager_, "test", logger_);

  router.Unload("phi-4:1");

  auto paths = server_.RequestedPaths();
  ASSERT_EQ(paths.size(), 1u);
  EXPECT_EQ(paths[0], "/models/unload/phi-4%3A1");
}

TEST_F(RouterExternalTest, Load_NonSuccessStatusThrowsNetwork) {
  server_.SetResponse(500, R"({"error":"boom"})");
  ModelCommandRouter router(server_.BaseUrl(), load_manager_, "test", logger_);

  try {
    router.Load("phi-4:1", "/ignored", ExecutionProvider::kCPU);
    FAIL() << "Expected fl::Exception on non-2xx response";
  } catch (const fl::Exception& e) {
    EXPECT_EQ(e.code(), FOUNDRY_LOCAL_ERROR_NETWORK);
  }
}

TEST_F(RouterExternalTest, ListLoadedModelIds_NonSuccessStatusThrowsNetwork) {
  server_.SetResponse(404, "not found");
  ModelCommandRouter router(server_.BaseUrl(), load_manager_, "test", logger_);

  try {
    router.ListLoadedModelIds();
    FAIL() << "Expected fl::Exception on non-2xx response";
  } catch (const fl::Exception& e) {
    EXPECT_EQ(e.code(), FOUNDRY_LOCAL_ERROR_NETWORK);
  }
}

TEST_F(RouterExternalTest, Unload_NonSuccessStatusThrowsNetwork) {
  server_.SetResponse(503, "unavailable");
  ModelCommandRouter router(server_.BaseUrl(), load_manager_, "test", logger_);

  try {
    router.Unload("phi-4:1");
    FAIL() << "Expected fl::Exception on non-2xx response";
  } catch (const fl::Exception& e) {
    EXPECT_EQ(e.code(), FOUNDRY_LOCAL_ERROR_NETWORK);
  }
}

TEST(RouterExternalTransportTest, TransportFailureThrowsNetwork) {
  // Port 1 on loopback has no listener — the connection is refused, which the router must surface
  // as FOUNDRY_LOCAL_ERROR_NETWORK (status == 0 transport failure), not crash or hang.
  test::CpuOnlyEpDetector ep;
  StderrLogger logger;
  ModelLoadManager load_manager(ep, logger);
  ModelCommandRouter router("http://127.0.0.1:1", load_manager, "test", logger);

  try {
    router.ListLoadedModelIds();
    FAIL() << "Expected fl::Exception on transport failure";
  } catch (const fl::Exception& e) {
    EXPECT_EQ(e.code(), FOUNDRY_LOCAL_ERROR_NETWORK);
  }
}

}  // namespace

// ========================================================================
// Real-web-service round-trip — external-mode router pointed at a live
// Foundry Local WebService. Mirrors v1's TestModelLoadManagerExternalService.
// Compiled only when the web service is built into the binary.
// ========================================================================

#ifdef FOUNDRY_LOCAL_HAS_WEB_SERVICE

#include "internal_api/web_service_test_helpers.h"
#include "service/web_service.h"
#include "inferencing/session/session_manager.h"
#include "null_telemetry.h"

#include <filesystem>

namespace {

// BaseModelCatalog subclass used to verify the GetLoadedModels() external-mode regression: it is
// wired with the *external* router, so GetLoadedModels() must reflect models loaded over HTTP.
class ExternalRouterCatalog : public BaseModelCatalog {
 public:
  ExternalRouterCatalog(ModelCommandRouter& router, ILogger& logger)
      : BaseModelCatalog("external-router-catalog", router, logger) {}

  void AddModel(Model model) { models_.push_back(std::move(model)); }

 protected:
  std::vector<Model> FetchModels() const override { return std::move(models_); }

 private:
  mutable std::vector<Model> models_;
};

// Starts a real WebService backed by a real ModelLoadManager and a catalog containing the tiny
// loadable test model. A separate, external-mode ModelCommandRouter is pointed at the bound URL so
// the tests drive the remote /models/* endpoints through the router under test.
class RouterWebServiceRoundTripTest : public ::testing::Test {
 protected:
  void SetUp() override {
    loadable_path_ = test::GetTestDataModelPath(test::kLoadableTestModelAlias);
    if (!std::filesystem::exists(loadable_path_)) {
      GTEST_SKIP() << "Loadable test model not present at " << loadable_path_
                   << "; skipping external-service round-trip.";
    }

    logger_ = std::make_unique<StderrLogger>();
    ep_detector_ = std::make_unique<test::CpuOnlyEpDetector>();
    server_load_manager_ = std::make_unique<ModelLoadManager>(*ep_detector_, *logger_);
    session_manager_ = std::make_unique<SessionManager>(*logger_);
    null_telemetry_ = std::make_unique<test::NullTelemetry>();

    // Local-mode router that the WebService catalog uses for its own bookkeeping.
    server_router_ = std::make_unique<ModelCommandRouter>(std::nullopt, *server_load_manager_, "test", *logger_);

    server_catalog_ = std::make_unique<test::MockCatalog>();
    server_catalog_->AddModel(Model::FromModelInfo(
        test::MakeTestModelInfo(test::kLoadableTestModelAlias, "microsoft"),
        loadable_path_, svc_.download_manager, *server_router_));

    service_ = std::make_unique<WebService>(*server_catalog_, *logger_, "/tmp/router-test-cache",
                                            *server_load_manager_, *session_manager_, *null_telemetry_, []() {});
    auto urls = service_->Start({"http://127.0.0.1:0"});
    ASSERT_EQ(urls.size(), 1u);
    base_url_ = urls[0];

    // The router under test talks to the service over HTTP in external mode. It wraps a *second*,
    // unused ModelLoadManager so the local branch is never accidentally taken.
    client_load_manager_ = std::make_unique<ModelLoadManager>(*ep_detector_, *logger_);
    external_router_ = std::make_unique<ModelCommandRouter>(base_url_, *client_load_manager_, "test", *logger_);
  }

  void TearDown() override {
    if (service_) {
      service_->Stop();
    }
    external_router_.reset();
    client_load_manager_.reset();
    service_.reset();
    server_catalog_.reset();
    server_router_.reset();
    null_telemetry_.reset();
    session_manager_.reset();
    server_load_manager_.reset();
    ep_detector_.reset();
    logger_.reset();
  }

  // The id reported by the web service for the loadable alias, e.g. "tiny-random-gpt2-fp32-1:1".
  std::string LoadedId() const { return std::string(test::kLoadableTestModelAlias) + ":1"; }

  std::string loadable_path_;
  std::string base_url_;
  std::unique_ptr<StderrLogger> logger_;
  std::unique_ptr<test::CpuOnlyEpDetector> ep_detector_;
  std::unique_ptr<ModelLoadManager> server_load_manager_;
  std::unique_ptr<ModelLoadManager> client_load_manager_;
  std::unique_ptr<SessionManager> session_manager_;
  std::unique_ptr<test::NullTelemetry> null_telemetry_;
  std::unique_ptr<ModelCommandRouter> server_router_;
  std::unique_ptr<ModelCommandRouter> external_router_;
  std::unique_ptr<test::MockCatalog> server_catalog_;
  std::unique_ptr<WebService> service_;
  test::FakeServiceBindings svc_;
};

TEST_F(RouterWebServiceRoundTripTest, LoadListUnloadRoundTripThroughExternalRouter) {
  // Nothing loaded initially.
  EXPECT_TRUE(external_router_->ListLoadedModelIds().empty());
  EXPECT_FALSE(external_router_->IsLoaded(LoadedId()));

  // Load over the external route. local_path / ep are ignored in external mode; the web service
  // resolves the alias against its own catalog and loads from disk.
  external_router_->Load(test::kLoadableTestModelAlias, loadable_path_, ExecutionProvider::kCPU);

  {
    auto ids = external_router_->ListLoadedModelIds();
    ASSERT_EQ(ids.size(), 1u) << "Expected exactly the loadable model to be reported as loaded";
    EXPECT_EQ(ids[0], LoadedId());
  }

  EXPECT_TRUE(external_router_->IsLoaded(LoadedId()));

  // Unload over the external route and confirm it is gone.
  external_router_->Unload(test::kLoadableTestModelAlias);

  EXPECT_TRUE(external_router_->ListLoadedModelIds().empty());
  EXPECT_FALSE(external_router_->IsLoaded(LoadedId()));
}

TEST_F(RouterWebServiceRoundTripTest, GetLoadedModelsReflectsExternalRouteLoads) {
  // A BaseModelCatalog wired with the external router. Before the fix this path always returned
  // empty in external mode because it issued per-model local IsLoaded() checks.
  ExternalRouterCatalog catalog(*external_router_, *logger_);
  catalog.AddModel(Model::FromModelInfo(
      test::MakeTestModelInfo(test::kLoadableTestModelAlias, "microsoft"),
      loadable_path_, svc_.download_manager, *external_router_));

  EXPECT_TRUE(catalog.GetLoadedModels().empty()) << "Nothing loaded yet";

  external_router_->Load(test::kLoadableTestModelAlias, loadable_path_, ExecutionProvider::kCPU);

  {
    auto loaded = catalog.GetLoadedModels();
    ASSERT_EQ(loaded.size(), 1u)
        << "GetLoadedModels() must reflect the model loaded over the external route";
    EXPECT_EQ(loaded[0]->Id(), LoadedId());
  }

  external_router_->Unload(test::kLoadableTestModelAlias);

  EXPECT_TRUE(catalog.GetLoadedModels().empty()) << "Model should be gone after external unload";
}

}  // namespace

#endif  // FOUNDRY_LOCAL_HAS_WEB_SERVICE
