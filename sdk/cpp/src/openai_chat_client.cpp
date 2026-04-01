// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <string>
#include <string_view>
#include <cstdint>
#include <ctime>

#include <gsl/span>
#include <nlohmann/json.hpp>

#include "foundry_local.h"
#include "foundry_local_internal_core.h"
#include "foundry_local_exception.h"
#include "core_interop_request.h"
#include "core_helpers.h"
#include "parser.h"
#include "logger.h"

namespace foundry_local {

std::string ChatCompletionCreateResponse::GetCreatedAtIso() const {
    if (created == 0)
        return {};
    std::time_t t = static_cast<std::time_t>(created);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

OpenAIChatClient::OpenAIChatClient(gsl::not_null<Internal::IFoundryLocalCore*> core, std::string_view modelId,
                                   gsl::not_null<ILogger*> logger)
    : core_(core), modelId_(modelId), logger_(logger) {}

std::string OpenAIChatClient::BuildChatRequestJson(gsl::span<const ChatMessage> messages,
                                                   gsl::span<const ToolDefinition> tools,
                                                   const ChatSettings& settings, bool stream) const {
    nlohmann::json jMessages = nlohmann::json::array();
    for (const auto& msg : messages) {
        nlohmann::json jMsg = {{"role", msg.role}, {"content", msg.content}};
        if (msg.tool_call_id)
            jMsg["tool_call_id"] = *msg.tool_call_id;
        jMessages.push_back(std::move(jMsg));
    }

    nlohmann::json req = {{"model", modelId_}, {"messages", std::move(jMessages)}, {"stream", stream}};

    if (!tools.empty()) {
        nlohmann::json jTools = nlohmann::json::array();
        for (const auto& tool : tools) {
            nlohmann::json jTool;
            to_json(jTool, tool);
            jTools.push_back(std::move(jTool));
        }
        req["tools"] = std::move(jTools);
    }

    if (settings.tool_choice)
        req["tool_choice"] = ParsingUtils::tool_choice_to_string(*settings.tool_choice);
    if (settings.top_k)
        req["metadata"] = {{"top_k", *settings.top_k}};
    if (settings.frequency_penalty)
        req["frequency_penalty"] = *settings.frequency_penalty;
    if (settings.presence_penalty)
        req["presence_penalty"] = *settings.presence_penalty;
    if (settings.max_tokens)
        req["max_completion_tokens"] = *settings.max_tokens;
    if (settings.n)
        req["n"] = *settings.n;
    if (settings.temperature)
        req["temperature"] = *settings.temperature;
    if (settings.top_p)
        req["top_p"] = *settings.top_p;
    if (settings.random_seed)
        req["seed"] = *settings.random_seed;

    return req.dump();
}

ChatCompletionCreateResponse OpenAIChatClient::CompleteChat(gsl::span<const ChatMessage> messages,
                                                            const ChatSettings& settings) const {
    return CompleteChat(messages, {}, settings);
}

ChatCompletionCreateResponse OpenAIChatClient::CompleteChat(gsl::span<const ChatMessage> messages,
                                                            gsl::span<const ToolDefinition> tools,
                                                            const ChatSettings& settings) const {
    std::string openAiReqJson = BuildChatRequestJson(messages, tools, settings, /*stream=*/false);

    CoreInteropRequest req("chat_completions");
    req.AddParam("OpenAICreateRequest", openAiReqJson);

    std::string json = req.ToJson();
    auto response = core_->call(req.Command(), *logger_, &json);
    if (response.HasError()) {
        throw Exception("Chat completion failed: " + response.error, *logger_);
    }

    return nlohmann::json::parse(response.data).get<ChatCompletionCreateResponse>();
}

void OpenAIChatClient::CompleteChatStreaming(gsl::span<const ChatMessage> messages, const ChatSettings& settings,
                                             const StreamCallback& onChunk) const {
    CompleteChatStreaming(messages, {}, settings, onChunk);
}

void OpenAIChatClient::CompleteChatStreaming(gsl::span<const ChatMessage> messages,
                                             gsl::span<const ToolDefinition> tools, const ChatSettings& settings,
                                             const StreamCallback& onChunk) const {
    std::string openAiReqJson = BuildChatRequestJson(messages, tools, settings, /*stream=*/true);

    CoreInteropRequest req("chat_completions");
    req.AddParam("OpenAICreateRequest", openAiReqJson);
    std::string json = req.ToJson();

    detail::CallWithStreamingCallback(
        core_, req.Command(), json, *logger_,
        [&onChunk](const std::string& chunk) {
            auto parsed = nlohmann::json::parse(chunk).get<ChatCompletionCreateResponse>();
            onChunk(parsed);
        },
        "Streaming chat completion failed: ");
}

OpenAIChatClient::OpenAIChatClient(const IModel& model)
    : OpenAIChatClient(model.GetCoreAccess().core, model.GetCoreAccess().modelName, model.GetCoreAccess().logger) {
    if (!model.IsLoaded()) {
        throw Exception("Model " + model.GetCoreAccess().modelName + " is not loaded. Call Load() first.",
                        *model.GetCoreAccess().logger);
    }
}

} // namespace foundry_local
