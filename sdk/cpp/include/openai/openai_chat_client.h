// Copyright (c) Microsoft Corporation. All rights reserved.
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include <optional>
#include <functional>

#include <gsl/pointers>
#include <gsl/span>

#include "openai_tool_types.h"

namespace FoundryLocal::Internal {
    struct IFoundryLocalCore;
}

namespace FoundryLocal {
    class ILogger;
    class ModelVariant;

    /// Reason the model stopped generating tokens.
    enum class FinishReason {
        None,
        Stop,
        Length,
        ToolCalls,
        ContentFilter
    };

    struct ChatMessage {
        std::string role;
        std::string content;
        std::optional<std::string> tool_call_id;  ///< For role="tool" responses
        std::vector<ToolCall> tool_calls;
    };

    struct ChatChoice {
        int index = 0;
        FinishReason finish_reason = FinishReason::None;

        // non-streaming
        std::optional<ChatMessage> message;

        // streaming
        std::optional<ChatMessage> delta;
    };

    struct ChatCompletionCreateResponse {
        int64_t created = 0;
        std::string id;

        bool is_delta = false;
        bool successful = false;
        int http_status_code = 0;

        std::vector<ChatChoice> choices;

        /// Returns the object type string. Derived from is_delta — no allocation.
        const char* GetObject() const noexcept { return is_delta ? "chat.completion.chunk" : "chat.completion"; }

        /// Returns the created timestamp as an ISO 8601 string.
        /// Computed lazily, only allocates when called.
        std::string GetCreatedAtIso() const;
    };

    struct ChatSettings {
        std::optional<float> frequency_penalty;
        std::optional<int> max_tokens;
        std::optional<int> n;
        std::optional<float> temperature;
        std::optional<float> presence_penalty;
        std::optional<int> random_seed;
        std::optional<int> top_k;
        std::optional<float> top_p;
        std::optional<ToolChoiceKind> tool_choice;
    };

    class OpenAIChatClient final {
    public:
        explicit OpenAIChatClient(gsl::not_null<const ModelVariant*> model);

        /// Returns the model ID this client was created for.
        const std::string& GetModelId() const noexcept { return modelId_; }

        ChatCompletionCreateResponse CompleteChat(gsl::span<const ChatMessage> messages,
                                                  const ChatSettings& settings) const;

        ChatCompletionCreateResponse CompleteChat(gsl::span<const ChatMessage> messages,
                                                  gsl::span<const ToolDefinition> tools,
                                                  const ChatSettings& settings) const;

        using StreamCallback = std::function<void(const ChatCompletionCreateResponse& chunk)>;
        void CompleteChatStreaming(gsl::span<const ChatMessage> messages, const ChatSettings& settings,
                                   const StreamCallback& onChunk) const;

        void CompleteChatStreaming(gsl::span<const ChatMessage> messages, gsl::span<const ToolDefinition> tools,
                                   const ChatSettings& settings, const StreamCallback& onChunk) const;

    private:
        OpenAIChatClient(gsl::not_null<FoundryLocal::Internal::IFoundryLocalCore*> core, std::string_view modelId,
                   gsl::not_null<ILogger*> logger);

        std::string BuildChatRequestJson(gsl::span<const ChatMessage> messages, gsl::span<const ToolDefinition> tools,
                                         const ChatSettings& settings, bool stream) const;

        std::string modelId_;
        gsl::not_null<FoundryLocal::Internal::IFoundryLocalCore*> core_;
        gsl::not_null<ILogger*> logger_;

        friend class ModelVariant;
    };

    /// Backward-compatible alias.
    using ChatClient = OpenAIChatClient;

} // namespace FoundryLocal
