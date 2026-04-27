// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>

#include <gsl/pointers>
#include <gsl/span>

namespace foundry_local::Internal {
    struct IFoundryLocalCore;
}

namespace foundry_local {
    class ILogger;
    class IModel;

    struct EmbeddingObject {
        int index = 0;
        std::vector<float> embedding;
    };

    struct EmbeddingUsage {
        std::optional<int> prompt_tokens;
        std::optional<int> total_tokens;
    };

    struct EmbeddingCreateResponse {
        std::string model;
        std::string object; ///< Always "list"
        std::vector<EmbeddingObject> data;
        std::optional<EmbeddingUsage> usage;
    };

    class OpenAIEmbeddingClient final {
    public:
        explicit OpenAIEmbeddingClient(const IModel& model);

        /// Returns the model ID this client was created for.
        const std::string& GetModelId() const noexcept { return modelId_; }

        /// Generate embedding for a single input string.
        EmbeddingCreateResponse GenerateEmbedding(std::string_view input) const;

        /// Generate embeddings for multiple input strings in a single request.
        EmbeddingCreateResponse GenerateEmbeddings(gsl::span<const std::string> inputs) const;

    private:
        OpenAIEmbeddingClient(gsl::not_null<foundry_local::Internal::IFoundryLocalCore*> core, std::string_view modelId,
                              gsl::not_null<ILogger*> logger);

        std::string BuildSingleRequestJson(std::string_view input) const;
        std::string BuildBatchRequestJson(gsl::span<const std::string> inputs) const;

        std::string modelId_;
        gsl::not_null<foundry_local::Internal::IFoundryLocalCore*> core_;
        gsl::not_null<ILogger*> logger_;
    };

} // namespace foundry_local
