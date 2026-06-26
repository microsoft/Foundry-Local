// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// External-mode (client/server split) integration tests.
//
// Foundry Local supports an "external service URL" mode where a Manager acts as a thin client that
// forwards model lifecycle operations to a separate process running the embedded web service. The
// service exposes three model-lifecycle routes — GET /models/load/{model}, GET /models/unload/{model},
// and GET /models/loaded; The SDK's public Model API issues these operations using the model_id
// (e.g. "tiny-random-gpt2-fp32:1") — see ModelCommandRouter, which forwards info_.model_id to
// GET /models/{load,unload}/{model}. The web service resolves {model} by model_id or alias for
// backwards compatibility.
//
// Why a separate process: foundry_local::Manager is a singleton — only one instance can exist at a
// time — so the web-service host (non-cache-only) and the external-mode client (cache-only) cannot
// coexist in the same process. The test binary re-executes itself with `--serve` to host the
// service, then runs the GTest suite in the parent process pointed at that server.
//
// Both processes share one cache directory containing a tiny, no-network-required model. The server
// (online) surfaces it in its catalog (via the cached-model merge path) and writes the catalog cache
// file the cache-only client reads, so both agree on the same model_id. When the host is offline or
// otherwise can't surface the model, the tests skip gracefully (consistent with the CI test policy).

#include <foundry_local/foundry_local_cpp.h>

#include <gtest/gtest.h>
#include <httplib.h>
#include <reproc++/reproc.hpp>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
// httplib transitively includes <windows.h>, which defines StartService as a macro
// (StartServiceA/StartServiceW). Undefine it so it can't shadow any foundry_local API identifier.
#undef StartService
#endif

namespace fs = std::filesystem;

namespace {

// The tiny model's scanned model_id. LocalModelScanner derives this from the "Name" field of the
// model's inference_model.json; that value already carries a ':' version suffix, so the scanner uses
// it verbatim (it only appends ":0" when no ':' is present). Note this differs from the on-disk
// directory / alias name "tiny-random-gpt2-fp32-1".
constexpr const char* kTinyModelDirName = "tiny-random-gpt2-fp32-1";
constexpr const char* kTinyModelId = "tiny-random-gpt2-fp32:1";

// Marker the host process prints once the web service is listening, followed by the URL it bound to.
// The host binds an ephemeral port (port 0), so the parent learns the actual port by parsing this
// line from the host's stdout.
constexpr const char* kServerReadyMarker = "EXTERNAL_MODE_SERVER_READY";

// Shared handle describing the spawned web-service host, populated by the global test environment.
struct ExternalServer {
  bool available = false;  ///< True once the host is reachable.
  std::string url;         ///< Base URL the host bound to, e.g. "http://127.0.0.1:54321".
  std::string cache_dir;   ///< Cache dir shared by host + client (contains the tiny model).
  std::string exe_path;    ///< This test binary's path, used to spawn additional hosts in tests.

  static ExternalServer& Get() {
    static ExternalServer instance;
    return instance;
  }
};

// --------------------------------------------------------------------------------------------------
// Host (`--serve`) mode
// --------------------------------------------------------------------------------------------------

int RunServer(int port, const std::string& cache_dir) {
  try {
    foundry_local::Configuration config("external_mode_test_server");
    config.SetModelCacheDir(cache_dir)
        .AddWebServiceEndpoint("http://127.0.0.1:" + std::to_string(port));

    foundry_local::Manager manager(std::move(config));
    manager.StartWebService();

    // With port 0 the OS assigns an ephemeral port; report the URL the service actually bound to so
    // the parent can connect to it.
    auto endpoints = manager.GetWebServiceEndpoints();
    if (endpoints.empty()) {
      std::cerr << "external-mode host: web service reported no endpoints" << std::endl;
      return 1;
    }

    // std::endl flushes, so the parent sees this line immediately.
    std::cout << kServerReadyMarker << " " << endpoints.front() << std::endl;

    // Block until shutdown is requested (e.g. via GET /shutdown) or the parent terminates us. A
    // bounded lifetime guards against orphaned hosts if the parent crashes before tear-down.
    constexpr int kMaxLifetimeSeconds = 900;
    for (int elapsed = 0; elapsed < kMaxLifetimeSeconds && !manager.IsShutdownRequested(); ++elapsed) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // GET /shutdown only raises the shutdown flag (Manager::RequestShutdown); the loop above observes
    // IsShutdownRequested() and breaks. Returning destroys the local `manager`, whose destructor runs
    // the actual teardown (stop web service, drain, unload) on this thread — the one safe place to do
    // it. There's nothing to stop explicitly here.
    return 0;
  } catch (const std::exception& ex) {
    // Web service support is a compile-time option. Surface the reason and exit; the parent sees the
    // stdout pipe hit EOF without a ready marker and the tests skip.
    std::cerr << "external-mode host failed to start: " << ex.what() << std::endl;
    return 1;
  }
}

// --------------------------------------------------------------------------------------------------
// Process spawning / teardown (parent side)
// --------------------------------------------------------------------------------------------------

// Spawns the host with `--port 0`, piping its stdout so the parent can read back the single ready
// line (marker + ephemeral URL). The host's stderr is inherited by this process so its logs stay
// visible. reproc handles the platform-specific process and pipe plumbing; on success the running
// child is owned by `out`.
bool SpawnServer(reproc::process& out, const std::string& exe_path, const std::string& cache_dir) {
  const std::vector<std::string> args = {exe_path, "--serve", "--port", "0", "--cache", cache_dir};

  reproc::options options;
  // The host prints exactly one line to stdout (the ready marker + URL); read it back through a pipe.
  // Everything else — SDK logs, host diagnostics — goes to stderr, which we inherit so it surfaces in
  // the test console without anyone having to drain a second pipe.
  options.redirect.out.type = reproc::redirect::pipe;
  options.redirect.err.type = reproc::redirect::parent;

  // Safety net: if the process somehow outlives `out`, terminate (then kill) it on destruction so a
  // crashed or aborted test can't leak a host process.
  options.stop = {{reproc::stop::terminate, reproc::milliseconds(2000)},
                  {reproc::stop::kill, reproc::milliseconds(2000)},
                  {reproc::stop::noop, reproc::milliseconds(0)}};

  std::error_code ec = out.start(args, options);
  if (ec) {
    std::cerr << "failed to spawn host process: " << ec.message() << std::endl;
    return false;
  }

  return true;
}

// Reads the host's single line of stdout — the ready marker followed by the URL it bound to — and
// returns that URL. Returns empty if the host exits before printing the line or the timeout elapses.
std::string ReadServerUrl(reproc::process& server, std::chrono::seconds timeout) {
  std::string buffer;
  const auto deadline = std::chrono::steady_clock::now() + timeout;

  while (buffer.find('\n') == std::string::npos) {
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) {
      return {};
    }

    const auto remaining = std::chrono::duration_cast<reproc::milliseconds>(deadline - now);

    // Block until the host produces output or exits, bounded by the remaining timeout.
    auto [events, poll_ec] = server.poll(reproc::event::out | reproc::event::exit, remaining);
    if (poll_ec) {
      return {};
    }

    if (events & reproc::event::out) {
      uint8_t chunk[256];
      auto [got, read_ec] = server.read(reproc::stream::out, chunk, sizeof(chunk));
      if (read_ec) {
        return {};  // stdout closed — the host exited before printing the marker.
      }
      buffer.append(reinterpret_cast<const char*>(chunk), got);
    } else if (events & reproc::event::exit) {
      return {};  // Host exited before printing the marker.
    }
  }

  // The line is "<marker> <url>". operator>> skips whitespace and stops at it, so a trailing '\r'
  // from Windows CRLF translation is dropped automatically.
  std::istringstream iss(buffer);
  std::string marker;
  std::string url;
  iss >> marker >> url;
  return marker == kServerReadyMarker ? url : std::string{};
}

// Asks the host to shut down gracefully via GET /shutdown and reports whether it accepted the request
// (HTTP 200 with a "shutting_down" status). The host then drains and exits on its own.
bool RequestGracefulShutdown(const std::string& url) {
  httplib::Client client(url);
  client.set_connection_timeout(2, 0);
  client.set_read_timeout(5, 0);

  auto res = client.Get("/shutdown");
  if (!res || res->status != 200) {
    return false;
  }

  return res->body.find("shutting_down") != std::string::npos;
}

// Waits up to timeout for the host process to exit on its own. Returns true if it exited within the
// window. reproc reaps the process, so a later TerminateServer is a no-op.
bool WaitForServerExit(reproc::process& server, std::chrono::milliseconds timeout) {
  auto [status, ec] = server.wait(std::chrono::duration_cast<reproc::milliseconds>(timeout));
  (void)status;
  return !ec;
}

// Forcefully stops the host if it is still running (graceful terminate, then kill) and reaps it.
// No-op if the process already exited.
void TerminateServer(reproc::process& server) {
  server.stop({{reproc::stop::terminate, reproc::milliseconds(2000)},
               {reproc::stop::kill, reproc::milliseconds(2000)},
               {reproc::stop::noop, reproc::milliseconds(0)}});
}

// Wait until the host answers GET /models/loaded with HTTP 200 (service up AND catalog ready), or
// the timeout elapses.
bool WaitForServerReady(const std::string& url, std::chrono::seconds timeout) {
  httplib::Client client(url);
  client.set_connection_timeout(2, 0);
  client.set_read_timeout(5, 0);

  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    auto res = client.Get("/models/loaded");
    if (res && res->status == 200) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return false;
}

// Locate the tiny model source directory copied next to the binary at build time.
fs::path TinyModelSourceDir() {
#ifdef FOUNDRY_LOCAL_TEST_DATA_DIR
  return fs::path(FOUNDRY_LOCAL_TEST_DATA_DIR) / kTinyModelDirName;
#else
  return fs::path("testdata") / kTinyModelDirName;
#endif
}

// --------------------------------------------------------------------------------------------------
// Global environment: prepares the shared cache dir, spawns the host, probes readiness.
// --------------------------------------------------------------------------------------------------

class ExternalServerEnvironment : public ::testing::Environment {
 public:
  explicit ExternalServerEnvironment(std::string exe_path) : exe_path_(std::move(exe_path)) {}

  void SetUp() override {
    auto src = TinyModelSourceDir();
    if (!fs::exists(src)) {
      std::cerr << "external-mode tests: tiny model not found at " << src
                << " — tests will skip" << std::endl;
      return;
    }

    // Unique cache dir per run so concurrent or repeated runs don't collide.
    std::random_device rng;
    cache_dir_ = fs::temp_directory_path() / ("fl_external_mode_test_" + std::to_string(rng()));

    std::error_code ec;
    fs::remove_all(cache_dir_, ec);
    fs::create_directories(cache_dir_ / kTinyModelDirName, ec);
    fs::copy(src, cache_dir_ / kTinyModelDirName,
             fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    if (ec) {
      std::cerr << "external-mode tests: failed to stage tiny model: " << ec.message() << std::endl;
      return;
    }

    if (!SpawnServer(server_proc_, exe_path_, cache_dir_.string())) {
      return;
    }

    // The host binds an ephemeral port and prints the resulting URL; read it back before probing.
    std::string url = ReadServerUrl(server_proc_, std::chrono::seconds(120));
    if (url.empty()) {
      std::cerr << "external-mode tests: host did not report a ready URL — tests will skip" << std::endl;
      TerminateServer(server_proc_);
      return;
    }

    if (!WaitForServerReady(url, std::chrono::seconds(120))) {
      std::cerr << "external-mode tests: host did not become ready — tests will skip" << std::endl;
      TerminateServer(server_proc_);
      return;
    }

    auto& server = ExternalServer::Get();
    server.available = true;
    server.url = url;
    server.cache_dir = cache_dir_.string();
    server.exe_path = exe_path_;
  }

  void TearDown() override {
    auto& server = ExternalServer::Get();
    if (server.available) {
      // Prefer a graceful shutdown via the web endpoint; TerminateServer below is the forceful
      // fallback and also releases the process / pipe handles.
      if (RequestGracefulShutdown(server.url)) {
        WaitForServerExit(server_proc_, std::chrono::seconds(30));
      }
    }

    TerminateServer(server_proc_);

    if (!cache_dir_.empty()) {
      std::error_code ec;
      fs::remove_all(cache_dir_, ec);
    }
  }

 private:
  std::string exe_path_;
  fs::path cache_dir_;
  reproc::process server_proc_;
};

// --------------------------------------------------------------------------------------------------
// Tests
// --------------------------------------------------------------------------------------------------

// End-to-end regression guard for the external-mode model_id routing fix (PR #839): the public
// Model::Load/IsLoaded/Unload path forwards model_id to the remote web service, which must resolve
// it by id. Before the fix Load() 404'd for every real model_id in external mode.
TEST(ExternalModeModelLifecycle, LoadIsLoadedUnloadRoundTrip) {
  auto& server = ExternalServer::Get();
  if (!server.available) {
    GTEST_SKIP() << "external-mode web service host not available (offline or build without service)";
  }

  foundry_local::Configuration config("external_mode_test_client");
  config.SetModelCacheDir(server.cache_dir)
      .SetExternalServiceUrl(server.url);

  foundry_local::Manager manager(std::move(config));
  auto& catalog = manager.GetCatalog();

  auto model = catalog.GetModelVariant(kTinyModelId);
  if (!model) {
    GTEST_SKIP() << "tiny model not present in external catalog (host offline?) — id " << kTinyModelId;
  }

  // model_id must resolve and load end-to-end through the remote service.
  ASSERT_FALSE(model->IsLoaded());
  ASSERT_NO_THROW(model->Load());
  EXPECT_TRUE(model->IsLoaded());

  // The loaded list is reported as model_ids (Model::Id()); the model we just loaded must appear.
  bool found = false;
  for (const auto& loaded : catalog.GetLoadedModels()) {
    if (loaded->GetInfo().Id() == kTinyModelId) {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found) << "expected loaded list to contain '" << kTinyModelId << "'";

  // Unload by model_id must also resolve and succeed.
  ASSERT_NO_THROW(model->Unload());
  EXPECT_FALSE(model->IsLoaded());
}

// Real coverage for the graceful-shutdown contract: GET /shutdown must be accepted and must cause the
// host process to exit on its own, with no forceful kill. Uses a dedicated short-lived host so it is
// independent of the shared host other tests rely on (and robust to test ordering / shuffle).
TEST(ExternalModeWebService, ShutdownEndpointTerminatesHostGracefully) {
  auto& shared = ExternalServer::Get();
  if (!shared.available) {
    GTEST_SKIP() << "external-mode web service host not available (offline or build without service)";
  }

  reproc::process host;
  ASSERT_TRUE(SpawnServer(host, shared.exe_path, shared.cache_dir)) << "failed to spawn a dedicated host";

  std::string url = ReadServerUrl(host, std::chrono::seconds(120));
  if (url.empty()) {
    TerminateServer(host);
    GTEST_SKIP() << "dedicated host did not report a ready URL (offline?)";
  }
  ASSERT_TRUE(WaitForServerReady(url, std::chrono::seconds(120))) << "dedicated host did not become ready";

  // The endpoint must acknowledge the request...
  EXPECT_TRUE(RequestGracefulShutdown(url)) << "host rejected GET /shutdown";

  // ...and the host must then terminate on its own, with no forceful kill required.
  EXPECT_TRUE(WaitForServerExit(host, std::chrono::seconds(30))) << "host did not exit after GET /shutdown";

  TerminateServer(host);  // Releases handles; forcefully kills only if the host is somehow still alive.
}

}  // namespace

// --------------------------------------------------------------------------------------------------
// main — dispatches to host mode (`--serve`) or test mode.
// --------------------------------------------------------------------------------------------------

int main(int argc, char** argv) {
  int port = 0;
  std::string cache_dir;
  bool serve = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--serve") {
      serve = true;
    } else if (arg == "--port" && i + 1 < argc) {
      port = std::atoi(argv[++i]);
    } else if (arg == "--cache" && i + 1 < argc) {
      cache_dir = argv[++i];
    }
  }

  if (serve) {
    return RunServer(port, cache_dir);
  }

  ::testing::InitGoogleTest(&argc, argv);
  ::testing::AddGlobalTestEnvironment(new ExternalServerEnvironment(argv[0]));
  return RUN_ALL_TESTS();
}
