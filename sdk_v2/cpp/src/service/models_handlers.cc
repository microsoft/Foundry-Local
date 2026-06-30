// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifdef FOUNDRY_LOCAL_HAS_WEB_SERVICE
#include "service/models_handlers.h"

#include "catalog.h"
#include "model_info.h"
#include "service/handler_utils.h"
#include "service/web_service.h"
#include "telemetry/telemetry_action_tracker.h"
#include "utils.h"

#include <foundry_local/foundry_local_c.h>  // for FOUNDRY_LOCAL_MODEL_PROP_* constants

#include <fmt/format.h>

namespace fl {

namespace {

// Resolve a /models/{model} path segment to a model. The path segment is a model id or an alias —
// not a model "name" (a model's name is the id minus its ":version" suffix, i.e. id == name:version).
// The SDK's public Model API issues load / unload using the model_id (e.g.
// "phi-4-mini-instruct-generic-cpu:2"), while the CLI and legacy callers may pass an alias (e.g.
// "phi-4-mini-instruct"). Accept both: try the id index first, then the alias index. model_ids and
// aliases occupy disjoint namespaces, so there is no ambiguity. This keeps every /models/* operation
// id-addressable (matching GET /models/loaded, which reports model_ids) while preserving alias
// backward-compatibility.
Model* ResolveModelByIdOrAlias(ServiceContext& ctx, const std::string& model_id_or_alias) {
  if (auto* variant = ctx.catalog.GetModelVariant(model_id_or_alias)) {
    return variant;
  }

  return ctx.catalog.GetModel(model_id_or_alias);
}

}  // namespace

// ========================================================================
// Handler: GET /models/loaded
// ========================================================================

class ListLoadedModelsHandler : public HttpRequestHandler {
 public:
  explicit ListLoadedModelsHandler(ServiceContext& ctx) : ctx_(ctx) {}

  std::shared_ptr<OutgoingResponse> handle(const std::shared_ptr<IncomingRequest>&) override {
    auto loaded = ctx_.catalog.GetLoadedModels();
    nlohmann::json names = nlohmann::json::array();

    for (const auto* model : loaded) {
      names.push_back(model->Id());
    }

    return JsonResponse(Status::CODE_200, names);
  }

 private:
  ServiceContext& ctx_;
};

// ========================================================================
// Handler: GET /models/load/{model}
// ========================================================================

class LoadModelHandler : public HttpRequestHandler {
 public:
  explicit LoadModelHandler(ServiceContext& ctx) : ctx_(ctx) {}

  std::shared_ptr<OutgoingResponse> handle(const std::shared_ptr<IncomingRequest>& request) override {
    ActionTracker tracker(Action::kModelLoad, ctx_.telemetry);

    auto model_path_param = request->getPathVariable("model");
    if (!model_path_param) {
      return ErrorResponse(Status::CODE_400, "Missing model id or alias");
    }

    // The model_id is percent-encoded on the wire by the SDK's ModelCommandRouter (e.g. ':' -> '%3A'),
    // so decode before resolving. Without this every id-form value (the SDK's default) 404s.
    std::string model_id_or_alias = UrlDecode(model_path_param->c_str());
    auto* model = ResolveModelByIdOrAlias(ctx_, model_id_or_alias);

    if (!model) {
      return ErrorResponse(Status::CODE_404, "Model not found",
                           "No model matching '" + model_id_or_alias + "'");
    }

    if (model->IsLoaded()) {
      tracker.SetStatus(ActionStatus::kSkipped);

      return JsonResponse(Status::CODE_200, {{"status", "already_loaded"}});
    }

    if (!model->IsCached()) {
      return ErrorResponse(Status::CODE_400, "Model not cached", "Model must be downloaded before loading");
    }

    try {
      model->Load();
      tracker.SetModelId(model_id_or_alias);
      tracker.SetStatus(ActionStatus::kSuccess);

      ctx_.logger.Log(LogLevel::Information,
                      fmt::format("Model loaded via web service: {}", model_id_or_alias));
      return JsonResponse(Status::CODE_200, {{"status", "loaded"}});
    } catch (const std::exception& ex) {
      tracker.SetModelId(model_id_or_alias);
      tracker.RecordException(ex);

      ctx_.logger.Log(LogLevel::Error,
                      fmt::format("Failed to load model {}: {}", model_id_or_alias, ex.what()));
      return ErrorResponse(Status::CODE_500, "Load failed", ex.what());
    }
  }

 private:
  ServiceContext& ctx_;
};

// ========================================================================
// Handler: GET /models/unload/{model}
// ========================================================================

class UnloadModelHandler : public HttpRequestHandler {
 public:
  explicit UnloadModelHandler(ServiceContext& ctx) : ctx_(ctx) {}

  std::shared_ptr<OutgoingResponse> handle(const std::shared_ptr<IncomingRequest>& request) override {
    ActionTracker tracker(Action::kModelUnload, ctx_.telemetry);

    auto model_path_param = request->getPathVariable("model");
    if (!model_path_param) {
      return ErrorResponse(Status::CODE_400, "Missing model id or alias");
    }

    // The model_id is percent-encoded on the wire by the SDK's ModelCommandRouter (e.g. ':' -> '%3A'),
    // so decode before resolving. Without this every id-form value (the SDK's default) 404s.
    std::string model_id_or_alias = UrlDecode(model_path_param->c_str());
    auto* model = ResolveModelByIdOrAlias(ctx_, model_id_or_alias);

    if (!model) {
      return ErrorResponse(Status::CODE_404, "Model not found",
                           "No model matching '" + model_id_or_alias + "'");
    }

    if (!model->IsLoaded()) {
      tracker.SetStatus(ActionStatus::kSkipped);

      return JsonResponse(Status::CODE_200, {{"status", "not_loaded"}});
    }

    try {
      model->Unload();
      tracker.SetModelId(model_id_or_alias);
      tracker.SetStatus(ActionStatus::kSuccess);

      ctx_.logger.Log(LogLevel::Information,
                      fmt::format("Model unloaded via web service: {}", model_id_or_alias));
      return JsonResponse(Status::CODE_200, {{"status", "unloaded"}});
    } catch (const std::exception& ex) {
      tracker.SetModelId(model_id_or_alias);
      tracker.RecordException(ex);

      ctx_.logger.Log(LogLevel::Error,
                      fmt::format("Failed to unload model {}: {}", model_id_or_alias, ex.what()));
      return ErrorResponse(Status::CODE_500, "Unload failed", ex.what());
    }
  }

 private:
  ServiceContext& ctx_;
};

// ========================================================================
// Handler: GET /v1/models — OpenAI-compatible model list
// ========================================================================

class OpenAIListModelsHandler : public HttpRequestHandler {
 public:
  explicit OpenAIListModelsHandler(ServiceContext& ctx) : ctx_(ctx) {}

  std::shared_ptr<OutgoingResponse> handle(const std::shared_ptr<IncomingRequest>&) override {
    ActionTracker tracker(Action::kOpenAIModelList, ctx_.telemetry);

    auto models = ctx_.catalog.ListModels();
    nlohmann::json data = nlohmann::json::array();

    // List individual variants so the client knows exactly which model_id to use.
    for (const auto* model : models) {
      for (const auto* variant : model->Variants()) {
        const auto& info = variant->Info();
        int64_t created = 0;
        auto it = info.int_properties.find(FOUNDRY_LOCAL_MODEL_PROP_CREATED_AT_UNIX_INT);
        if (it != info.int_properties.end()) {
          created = it->second;
        }

        std::string owned_by = "system";
        auto pub_it = info.string_properties.find(FOUNDRY_LOCAL_MODEL_PROP_PUBLISHER_STR);
        if (pub_it != info.string_properties.end()) {
          owned_by = pub_it->second;
        }

        data.push_back({
            {"id", info.model_id},
            {"object", "model"},
            {"created", created},
            {"owned_by", owned_by},
        });
      }
    }

    nlohmann::json body = {
        {"object", "list"},
        {"data", data},
    };

    tracker.SetStatus(ActionStatus::kSuccess);

    return JsonResponse(Status::CODE_200, body);
  }

 private:
  ServiceContext& ctx_;
};

// ========================================================================
// Handler: GET /v1/models/{model} — OpenAI-compatible model retrieve
// ========================================================================

class OpenAIRetrieveModelHandler : public HttpRequestHandler {
 public:
  explicit OpenAIRetrieveModelHandler(ServiceContext& ctx) : ctx_(ctx) {}

  std::shared_ptr<OutgoingResponse> handle(const std::shared_ptr<IncomingRequest>& request) override {
    ActionTracker tracker(Action::kOpenAIModelRetrieve, ctx_.telemetry);

    auto model_path_param = request->getPathVariable("model");
    if (!model_path_param) {
      return ErrorResponse(Status::CODE_400, "Missing model id");
    }

    // Percent-decode the id so encoded path segments (e.g. 'name%3A1') resolve like 'name:1'.
    std::string model_id = UrlDecode(model_path_param->c_str());
    auto* model = ctx_.catalog.GetModelVariant(model_id);

    if (!model) {
      return ErrorResponse(Status::CODE_404, "Model not found", "No model matching '" + model_id + "'");
    }

    const auto& info = model->Info();
    int64_t created = 0;
    auto it = info.int_properties.find(FOUNDRY_LOCAL_MODEL_PROP_CREATED_AT_UNIX_INT);
    if (it != info.int_properties.end()) {
      created = it->second;
    }

    std::string owned_by = "system";
    auto pub_it = info.string_properties.find(FOUNDRY_LOCAL_MODEL_PROP_PUBLISHER_STR);
    if (pub_it != info.string_properties.end()) {
      owned_by = pub_it->second;
    }

    nlohmann::json body = {
        {"id", info.model_id},
        {"object", "model"},
        {"created", created},
        {"owned_by", owned_by},
    };

    tracker.SetStatus(ActionStatus::kSuccess);

    return JsonResponse(Status::CODE_200, body);
  }

 private:
  ServiceContext& ctx_;
};

// ========================================================================
// Factory functions
// ========================================================================

std::shared_ptr<oatpp::web::server::HttpRequestHandler> CreateListLoadedModelsHandler(ServiceContext& ctx) {
  return std::make_shared<ListLoadedModelsHandler>(ctx);
}

std::shared_ptr<oatpp::web::server::HttpRequestHandler> CreateLoadModelHandler(ServiceContext& ctx) {
  return std::make_shared<LoadModelHandler>(ctx);
}

std::shared_ptr<oatpp::web::server::HttpRequestHandler> CreateUnloadModelHandler(ServiceContext& ctx) {
  return std::make_shared<UnloadModelHandler>(ctx);
}

std::shared_ptr<oatpp::web::server::HttpRequestHandler> CreateOpenAIListModelsHandler(ServiceContext& ctx) {
  return std::make_shared<OpenAIListModelsHandler>(ctx);
}

std::shared_ptr<oatpp::web::server::HttpRequestHandler> CreateOpenAIRetrieveModelHandler(ServiceContext& ctx) {
  return std::make_shared<OpenAIRetrieveModelHandler>(ctx);
}

}  // namespace fl

#endif  // FOUNDRY_LOCAL_HAS_WEB_SERVICE
