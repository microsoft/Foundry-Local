// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Internal helpers shared across implementation files.
// Not part of the public API.

#pragma once

#include <charconv>
#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <exception>
#include <system_error>
#include <unordered_map>
#include <utility>

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

    inline bool TryParseFloatToken(std::string_view token, float& value) {
        if (token.empty()) {
            return false;
        }

        const auto* begin = token.data();
        const auto* end = begin + token.size();
        const auto result = std::from_chars(begin, end, value);
        return result.ec == std::errc{} && result.ptr == end;
    }

    inline bool TryParseDoubleToken(std::string_view token, double& value) {
        if (token.empty()) {
            return false;
        }

        const auto* begin = token.data();
        const auto* end = begin + token.size();
        const auto result = std::from_chars(begin, end, value);
        return result.ec == std::errc{} && result.ptr == end;
    }

    // Serialize + call with a streaming chunk handler.
    // Wraps the caller-supplied onChunk with the native callback boilerplate
    // (null/length checks, exception capture, cancellation, rethrow after the call).
    // The errorContext string is used to prefix any core-layer error message.
    inline CoreResponse CallWithStreamingCallback(Internal::IFoundryLocalCore* core, std::string_view command,
                                                   const std::string* payload, ILogger& logger,
                                                   const std::function<bool(const std::string&)>& onChunk,
                                                   std::string_view errorContext,
                                                   CancellationCallback isCancellationRequested = nullptr) {
        struct State {
            const std::function<bool(const std::string&)>* cb;
            CancellationCallback isCancellationRequested;
            bool cancellationObserved = false;
            std::exception_ptr exception;
        } state{&onChunk, std::move(isCancellationRequested), false, nullptr};

        auto nativeCallback = [](const void* data, int32_t len, void* user) -> int32_t {
            auto* st = static_cast<State*>(user);
            if (!st) {
                return 0;
            }

            if (st->exception || st->cancellationObserved) {
                return 1;
            }

            if (!data || len <= 0)
                return 0;

            try {
                if (st->isCancellationRequested && st->isCancellationRequested()) {
                    st->cancellationObserved = true;
                    return 1;
                }

                std::string chunk(static_cast<const char*>(data), static_cast<size_t>(len));
                if (!(*(st->cb))(chunk)) {
                    st->cancellationObserved = true;
                    return 1;
                }
            }
            catch (...) {
                st->exception = std::current_exception();
                return 1;
            }

            return 0;
        };

        auto response = core->call(command, logger, payload, +nativeCallback, &state);
        if (state.cancellationObserved) {
            throw Exception("Operation cancelled", logger);
        }

        if (response.HasError()) {
            throw Exception(std::string(errorContext) + response.error, logger);
        }

        if (state.exception) {
            std::rethrow_exception(state.exception);
        }

        return response;
    }

    inline CoreResponse CallWithStreamingCallback(Internal::IFoundryLocalCore* core, std::string_view command,
                                                   const std::string* payload, ILogger& logger,
                                                   const std::function<void(const std::string&)>& onChunk,
                                                   std::string_view errorContext,
                                                   CancellationCallback isCancellationRequested = nullptr) {
        const std::function<bool(const std::string&)> continuingOnChunk =
            [&onChunk](const std::string& chunk) {
                onChunk(chunk);
                return true;
            };
        return CallWithStreamingCallback(core, command, payload, logger, continuingOnChunk, errorContext,
                                         std::move(isCancellationRequested));
    }

    inline CoreResponse CallWithStreamingCallback(Internal::IFoundryLocalCore* core, std::string_view command,
                                                   const std::string& payload, ILogger& logger,
                                                   const std::function<bool(const std::string&)>& onChunk,
                                                   std::string_view errorContext,
                                                   CancellationCallback isCancellationRequested = nullptr) {
        return CallWithStreamingCallback(core, command, &payload, logger, onChunk, errorContext,
                                         std::move(isCancellationRequested));
    }

    inline CoreResponse CallWithStreamingCallback(Internal::IFoundryLocalCore* core, std::string_view command,
                                                   const std::string& payload, ILogger& logger,
                                                   const std::function<void(const std::string&)>& onChunk,
                                                   std::string_view errorContext,
                                                   CancellationCallback isCancellationRequested = nullptr) {
        return CallWithStreamingCallback(core, command, &payload, logger, onChunk, errorContext,
                                         std::move(isCancellationRequested));
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

    inline std::vector<IModel*> CollectVariantsByIds(
        const std::unordered_map<std::string, ModelVariant*>& modelIdToModelVariant, std::vector<std::string> ids) {
        std::vector<IModel*> out;
        out.reserve(ids.size());

        for (const auto& id : ids) {
            auto it = modelIdToModelVariant.find(id);
            if (it != modelIdToModelVariant.end()) {
                out.emplace_back(it->second);
            }
        }
        return out;
    }

} // namespace foundry_local::detail
