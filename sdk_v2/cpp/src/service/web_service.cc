// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifdef FOUNDRY_LOCAL_HAS_WEB_SERVICE

#include "service/web_service.h"

#include "exception.h"
#include "inferencing/generative/openresponses/response_store.h"
#include "service/audio_transcriptions_handler.h"
#include "service/chat_completions_handler.h"
#include "service/embeddings_handler.h"
#include "service/handler_utils.h"
#include "service/models_handlers.h"
#include "service/responses_handler.h"

#include <fmt/format.h>
#include <oatpp/network/Server.hpp>
#include <oatpp/network/tcp/server/ConnectionProvider.hpp>
#include <oatpp/web/server/HttpConnectionHandler.hpp>
#include <oatpp/web/server/HttpRouter.hpp>

#include <atomic>
#include <chrono>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace fl {

// ========================================================================
// Handler: GET /status
// ========================================================================

class StatusHandler : public HttpRequestHandler {
 public:
  explicit StatusHandler(ServiceContext& ctx) : ctx_(ctx) {}

  std::shared_ptr<OutgoingResponse> handle(const std::shared_ptr<IncomingRequest>&) override {
    nlohmann::json body = {
        {"modelCachePath", ctx_.model_cache_dir},
        {"endpoints", ctx_.bound_urls},
#ifdef _WIN32
        {"pid", static_cast<int64_t>(GetCurrentProcessId())},
#else
        {"pid", static_cast<int64_t>(getpid())},
#endif
    };

    return JsonResponse(Status::CODE_200, body);
  }

 private:
  ServiceContext& ctx_;
};

// ========================================================================
// Handler: POST /shutdown
// ========================================================================

class ShutdownHandler : public HttpRequestHandler {
 public:
  explicit ShutdownHandler(std::function<void()> shutdown_fn)
      : shutdown_fn_(std::move(shutdown_fn)) {}

  std::shared_ptr<OutgoingResponse> handle(const std::shared_ptr<IncomingRequest>&) override {
    shutdown_fn_();

    nlohmann::json body = {{"status", "shutting_down"}};
    return JsonResponse(Status::CODE_200, body);
  }

 private:
  std::function<void()> shutdown_fn_;
};

// ========================================================================
// WebService implementation
// ========================================================================

struct WebService::Impl {
  std::vector<std::shared_ptr<oatpp::network::Server>> servers;
  std::vector<std::thread> listener_threads;
  std::vector<std::shared_ptr<oatpp::network::tcp::server::ConnectionProvider>> providers;
  std::shared_ptr<oatpp::web::server::HttpConnectionHandler> connection_handler;
  std::shared_ptr<oatpp::web::server::HttpRouter> router;
  ResponseStore response_store;
  StreamingThreadTracker thread_tracker;
  std::function<void()> shutdown_callback;
  std::unique_ptr<ServiceContext> context;
  std::atomic<bool> running{false};

  Impl(ICatalog& catalog, ILogger& logger, std::string model_cache_dir,
       ModelLoadManager& model_load_manager, SessionManager& session_manager,
       ITelemetry& telemetry, std::function<void()> shutdown_callback)
      : shutdown_callback(std::move(shutdown_callback)),
        context(std::make_unique<ServiceContext>(
            ServiceContext{catalog,
                           logger,
                           std::move(model_cache_dir),
                           {},
                           model_load_manager,
                           session_manager,
                           response_store,
                           telemetry,
                           thread_tracker})) {}
};

WebService::WebService(ICatalog& catalog, ILogger& logger, std::string model_cache_dir,
                       ModelLoadManager& model_load_manager, SessionManager& session_manager,
                       ITelemetry& telemetry, std::function<void()> shutdown_callback)
    : impl_(std::make_unique<Impl>(catalog, logger, std::move(model_cache_dir),
                                   model_load_manager, session_manager, telemetry,
                                   std::move(shutdown_callback))) {}

WebService::~WebService() {
  if (impl_->running.load()) {
    Stop();
  }
}

std::vector<std::string> WebService::Start(const std::vector<std::string>& endpoints) {
  auto& ctx = *impl_->context;

  if (impl_->running.load()) {
    ctx.logger.Log(LogLevel::Information, "Web service is already running.");
    FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_USAGE, "Web service is already running");
  }

  // Create shared router and register all routes
  impl_->router = oatpp::web::server::HttpRouter::createShared();

  // Status
  impl_->router->route("GET", "/status",
                       std::make_shared<StatusHandler>(ctx));

  // Shutdown
  impl_->router->route("POST", "/shutdown",
                       std::make_shared<ShutdownHandler>(impl_->shutdown_callback));

  // Model management
  impl_->router->route("GET", "/models/loaded", CreateListLoadedModelsHandler(ctx));
  impl_->router->route("GET", "/models/load/{name}", CreateLoadModelHandler(ctx));
  impl_->router->route("GET", "/models/unload/{name}", CreateUnloadModelHandler(ctx));

  // OpenAI-compatible endpoints
  impl_->router->route("GET", "/v1/models", CreateOpenAIListModelsHandler(ctx));
  impl_->router->route("GET", "/v1/models/{name}", CreateOpenAIRetrieveModelHandler(ctx));
  impl_->router->route("POST", "/v1/chat/completions", CreateChatCompletionsHandler(ctx));
  impl_->router->route("POST", "/v1/audio/transcriptions", CreateAudioTranscriptionsHandler(ctx));
  impl_->router->route("POST", "/v1/embeddings", CreateEmbeddingsHandler(ctx));
  impl_->router->route("POST", "/v1/responses", CreateResponsesHandler(ctx));
  impl_->router->route("GET", "/v1/responses", CreateListResponsesHandler(ctx));
  impl_->router->route("GET", "/v1/responses/{id}", CreateGetResponseHandler(ctx));
  impl_->router->route("DELETE", "/v1/responses/{id}", CreateDeleteResponseHandler(ctx));
  impl_->router->route("GET", "/v1/responses/{id}/input_items", CreateGetInputItemsHandler(ctx));

  impl_->connection_handler = oatpp::web::server::HttpConnectionHandler::createShared(impl_->router);

  std::vector<std::string> bound_urls;

  for (const auto& endpoint : endpoints) {
    // Parse "http://host:port" — strip scheme, extract host:port
    std::string host = "127.0.0.1";
    uint16_t port = 0;

    std::string addr = endpoint;
    auto scheme_end = addr.find("://");
    if (scheme_end != std::string::npos) {
      addr = addr.substr(scheme_end + 3);
    }

    if (!addr.empty() && addr.back() == '/') {
      addr.pop_back();
    }

    auto colon = addr.rfind(':');
    if (colon != std::string::npos) {
      host = addr.substr(0, colon);
      port = static_cast<uint16_t>(std::stoi(addr.substr(colon + 1)));
    } else {
      host = addr;
    }

    auto provider = oatpp::network::tcp::server::ConnectionProvider::createShared({host.c_str(), port});

    // oatpp resolves ephemeral port 0 during construction via getsockname()
    // and stores it in PROPERTY_PORT. getAddress().port is stale — use the property.
    auto resolved_port = provider->getProperty(oatpp::network::ConnectionProvider::PROPERTY_PORT);

    if (resolved_port) {
      port = static_cast<uint16_t>(std::stoi(resolved_port.std_str()));
    }

    auto server = std::make_shared<oatpp::network::Server>(provider, impl_->connection_handler);

    impl_->providers.push_back(provider);
    impl_->servers.push_back(server);

    // Start server on a background thread
    impl_->listener_threads.emplace_back([server]() {
      server->run();
    });

    // Wait until oatpp's server reports STATUS_RUNNING (i.e. the listener loop
    // has started and is accepting connections). Bounded poll with a 5s timeout.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (server->getStatus() != oatpp::network::Server::STATUS_RUNNING &&
           std::chrono::steady_clock::now() < deadline) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (server->getStatus() != oatpp::network::Server::STATUS_RUNNING) {
      // Tear down anything we already started in this call so we don't leak
      // listener threads. `running` is still false, so the destructor would skip Stop().
      impl_->running.store(true);
      Stop();
      FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
               "Web service failed to reach RUNNING state for endpoint ", endpoint, " within 5s");
    }

    std::string bound_url = "http://" + host + ":" + std::to_string(port);
    bound_urls.push_back(bound_url);

    ctx.logger.Log(LogLevel::Information, fmt::format("Web service listening on {}", bound_url));
  }

  ctx.bound_urls = bound_urls;
  impl_->running.store(true);

  return bound_urls;
}

void WebService::Stop() {
  if (!impl_->running.load()) {
    return;
  }

  // Join streaming threads first — they may still be pushing to SSE bodies.
  impl_->thread_tracker.JoinAll();

  // Stop accepting new connections first, then stop server loops.
  for (auto& provider : impl_->providers) {
    provider->stop();
  }

  for (auto& server : impl_->servers) {
    server->stop();
  }

  // Stop per-connection worker tasks before releasing router/handlers.
  if (impl_->connection_handler) {
    impl_->connection_handler->stop();
  }

  for (auto& thread : impl_->listener_threads) {
    if (thread.joinable()) {
      thread.join();
    }
  }

  impl_->servers.clear();
  impl_->listener_threads.clear();
  impl_->providers.clear();
  impl_->connection_handler.reset();
  impl_->router.reset();
  impl_->running.store(false);

  impl_->context->bound_urls.clear();
  impl_->context->logger.Log(LogLevel::Information, "Web service stopped");
}

}  // namespace fl

#endif  // FOUNDRY_LOCAL_HAS_WEB_SERVICE
