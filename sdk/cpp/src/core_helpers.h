// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Internal helpers shared across implementation files.
// Not part of the public API.

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <exception>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "foundry_local_internal_core.h"
#include "foundry_local_exception.h"
#include "logger.h"
#include "model.h"

namespace foundry_local::detail {

    // Wrap Params: { ... } into a request object
    inline nlohmann::json MakeParams(nlohmann::json params) {
        return nlohmann::json{{"Params", std::move(params)}};
    }

    // Most common: Params { "Model": <idOrName> }
    inline nlohmann::json MakeModelParams(std::string_view model) {
        return MakeParams(nlohmann::json{{"Model", std::string(model)}});
    }

    // Serialize + call
    inline CoreResponse CallWithJson(Internal::IFoundryLocalCore* core, std::string_view command,
                                     const nlohmann::json& requestJson, ILogger& logger) {
        std::string payload = requestJson.dump();
        return core->call(command, logger, &payload);
    }

    // Serialize + call with native callback
    inline CoreResponse CallWithJsonAndCallback(Internal::IFoundryLocalCore* core, std::string_view command,
                                                const nlohmann::json& requestJson, ILogger& logger,
                                                NativeCallbackFn callback, void* userData) {
        std::string payload = requestJson.dump();
        return core->call(command, logger, &payload, callback, userData);
    }

    // Serialize + call with a streaming chunk handler.
    // Wraps the caller-supplied onChunk with the native callback boilerplate
    // (null/length checks, exception capture, rethrow after the call).
    // The errorContext string is used to prefix any core-layer error message.
    inline CoreResponse CallWithStreamingCallback(Internal::IFoundryLocalCore* core, std::string_view command,
                                                  const std::string& payload, ILogger& logger,
                                                  const std::function<void(const std::string&)>& onChunk,
                                                  std::string_view errorContext) {
        struct State {
            const std::function<void(const std::string&)>* cb;
            std::exception_ptr exception;
        } state{&onChunk, nullptr};

        auto nativeCallback = [](void* data, int32_t len, void* user) {
            if (!data || len <= 0)
                return;

            auto* st = static_cast<State*>(user);
            if (st->exception)
                return;

            try {
                std::string chunk(static_cast<const char*>(data), static_cast<size_t>(len));
                (*(st->cb))(chunk);
            }
            catch (...) {
                st->exception = std::current_exception();
            }
        };

        auto response = core->call(command, logger, &payload, +nativeCallback, &state);
        if (response.HasError()) {
            throw Exception(std::string(errorContext) + response.error, logger);
        }

        if (state.exception) {
            std::rethrow_exception(state.exception);
        }

        return response;
    }

    // Overload: allow Params object directly
    inline CoreResponse CallWithParams(Internal::IFoundryLocalCore* core, std::string_view command,
                                       const nlohmann::json& params, ILogger& logger) {
        return CallWithJson(core, command, MakeParams(params), logger);
    }

    // Overload: no payload
    inline CoreResponse CallNoArgs(Internal::IFoundryLocalCore* core, std::string_view command, ILogger& logger) {
        return core->call(command, logger, nullptr);
    }

    inline std::vector<std::string> GetLoadedModelsInternal(Internal::IFoundryLocalCore* core, ILogger& logger) {
        auto response = core->call("list_loaded_models", logger);
        if (response.HasError()) {
            throw Exception("Failed to get loaded models: " + response.error, logger);
        }
        try {
            auto parsed = nlohmann::json::parse(response.data);
            return parsed.get<std::vector<std::string>>();
        }
        catch (const nlohmann::json::exception& e) {
            throw Exception("Catalog::GetLoadedModelsInternal() JSON error: " + std::string(e.what()), logger);
        }
    }

    inline std::vector<std::string> GetCachedModelsInternal(Internal::IFoundryLocalCore* core, ILogger& logger) {
        auto response = core->call("get_cached_models", logger);
        if (response.HasError()) {
            throw Exception("Failed to get cached models: " + response.error, logger);
        }

        try {
            auto parsed = nlohmann::json::parse(response.data);
            return parsed.get<std::vector<std::string>>();
        }
        catch (const nlohmann::json::exception& e) {
            throw Exception("Catalog::GetCachedModelsInternal JSON error: " + std::string(e.what()), logger);
        }
    }

    inline std::vector<ModelVariant*> CollectVariantsByIds(
        const std::unordered_map<std::string, ModelVariant>& modelIdToModelVariant, std::vector<std::string> ids) {
        std::vector<ModelVariant*> out;
        out.reserve(ids.size());

        for (const auto& id : ids) {
            auto it = modelIdToModelVariant.find(id);
            if (it != modelIdToModelVariant.end()) {
                out.emplace_back(const_cast<ModelVariant*>(&it->second));
            }
        }
        return out;
    }

} // namespace foundry_local::detail
