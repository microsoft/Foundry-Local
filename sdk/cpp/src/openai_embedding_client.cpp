// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <cctype>
#include <string>
#include <string_view>
#include <vector>

#include <gsl/span>
#include <nlohmann/json.hpp>

#include "foundry_local.h"
#include "foundry_local_internal_core.h"
#include "foundry_local_exception.h"
#include "core_interop_request.h"
#include "parser.h"
#include "logger.h"

namespace foundry_local {

    namespace {
        /// True for strings that are empty or contain only whitespace characters.
        bool IsBlank(std::string_view s) {
            for (char c : s) {
                if (!std::isspace(static_cast<unsigned char>(c))) {
                    return false;
                }
            }
            return true;
        }
    } // namespace

    OpenAIEmbeddingClient::OpenAIEmbeddingClient(gsl::not_null<Internal::IFoundryLocalCore*> core,
                                                 std::string_view modelId, gsl::not_null<ILogger*> logger)
        : core_(core), modelId_(modelId), logger_(logger) {}

    std::string OpenAIEmbeddingClient::BuildSingleRequestJson(std::string_view input) const {
        nlohmann::json req = {{"model", modelId_}, {"input", std::string(input)}};
        return req.dump();
    }

    std::string OpenAIEmbeddingClient::BuildBatchRequestJson(gsl::span<const std::string> inputs) const {
        nlohmann::json jInputs = nlohmann::json::array();
        for (const auto& s : inputs) {
            jInputs.push_back(s);
        }
        nlohmann::json req = {{"model", modelId_}, {"input", std::move(jInputs)}};
        return req.dump();
    }

    EmbeddingCreateResponse OpenAIEmbeddingClient::GenerateEmbedding(std::string_view input) const {
        if (IsBlank(input)) {
            throw Exception("Embedding input must be a non-empty string.", *logger_);
        }

        std::string openAiReqJson = BuildSingleRequestJson(input);

        CoreInteropRequest req("embeddings");
        req.AddParam("OpenAICreateRequest", openAiReqJson);

        std::string json = req.ToJson();
        auto response = core_->call(req.Command(), *logger_, &json);
        if (response.HasError()) {
            throw Exception("Embedding generation failed: " + response.error, *logger_);
        }

        return nlohmann::json::parse(response.data).get<EmbeddingCreateResponse>();
    }

    EmbeddingCreateResponse OpenAIEmbeddingClient::GenerateEmbeddings(gsl::span<const std::string> inputs) const {
        if (inputs.empty()) {
            throw Exception("Embedding inputs must be a non-empty array of strings.", *logger_);
        }
        for (const auto& s : inputs) {
            if (IsBlank(s)) {
                throw Exception("Each embedding input must be a non-empty string.", *logger_);
            }
        }

        std::string openAiReqJson = BuildBatchRequestJson(inputs);

        CoreInteropRequest req("embeddings");
        req.AddParam("OpenAICreateRequest", openAiReqJson);

        std::string json = req.ToJson();
        auto response = core_->call(req.Command(), *logger_, &json);
        if (response.HasError()) {
            throw Exception("Batch embedding generation failed: " + response.error, *logger_);
        }

        return nlohmann::json::parse(response.data).get<EmbeddingCreateResponse>();
    }

    OpenAIEmbeddingClient::OpenAIEmbeddingClient(const IModel& model)
        : OpenAIEmbeddingClient(model.GetCoreAccess().core, model.GetCoreAccess().modelName,
                                model.GetCoreAccess().logger) {
        if (!model.IsLoaded()) {
            throw Exception("Model " + model.GetCoreAccess().modelName + " is not loaded. Call Load() first.",
                            *model.GetCoreAccess().logger);
        }
    }

} // namespace foundry_local
