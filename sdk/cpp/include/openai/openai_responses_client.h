// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <optional>

#include <gsl/pointers>
#include <gsl/span>

#include "openai_responses_types.h"

namespace foundry_local {
    class ILogger;

    /// Client for the OpenAI Responses API (HTTP-only, requires web service).
    /// Create via Manager::CreateResponsesClient() after StartWebService().
    class OpenAIResponsesClient final {
    public:
        OpenAIResponsesClient(std::string baseUrl, std::string modelId,
                              gsl::not_null<ILogger*> logger);

        const std::string& GetModelId() const noexcept { return modelId_; }

        ResponseSettings& GetSettings() noexcept { return settings_; }
        const ResponseSettings& GetSettings() const noexcept { return settings_; }

        // ── Create ───────────────────────────────────────────────────

        /// Non-streaming: text prompt or structured input items.
        ResponseObject Create(const std::string& input,
                              const std::optional<std::string>& previousResponseId = std::nullopt,
                              gsl::span<const ResponseFunctionToolDefinition> tools = {}) const;

        ResponseObject Create(gsl::span<const ResponseInputItem> input,
                              const std::optional<std::string>& previousResponseId = std::nullopt,
                              gsl::span<const ResponseFunctionToolDefinition> tools = {}) const;

        // ── Streaming ────────────────────────────────────────────────

        using StreamCallback = std::function<void(const StreamingEvent& event)>;

        void CreateStreaming(const std::string& input,
                             const StreamCallback& onEvent,
                             const std::optional<std::string>& previousResponseId = std::nullopt,
                             gsl::span<const ResponseFunctionToolDefinition> tools = {}) const;

        void CreateStreaming(gsl::span<const ResponseInputItem> input,
                             const StreamCallback& onEvent,
                             const std::optional<std::string>& previousResponseId = std::nullopt,
                             gsl::span<const ResponseFunctionToolDefinition> tools = {}) const;

        // ── CRUD ─────────────────────────────────────────────────────

        ResponseObject Get(const std::string& responseId) const;
        DeleteResponseResult Delete(const std::string& responseId) const;
        ResponseObject Cancel(const std::string& responseId) const;
        InputItemsListResponse GetInputItems(const std::string& responseId) const;

    private:
        std::string BuildRequestJson(const std::string& inputJson, bool stream,
                                     const std::optional<std::string>& previousResponseId,
                                     gsl::span<const ResponseFunctionToolDefinition> tools) const;

        std::string SerializeInputItems(gsl::span<const ResponseInputItem> input) const;

        std::string baseUrl_;
        std::string modelId_;
        gsl::not_null<ILogger*> logger_;
        ResponseSettings settings_;
    };

} // namespace foundry_local
