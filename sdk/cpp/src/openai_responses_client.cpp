// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <string>
#include <string_view>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "openai/openai_responses_client.h"
#include "openai/openai_responses_types.h"
#include "foundry_local_exception.h"
#include "http_helpers.h"
#include "logger.h"

namespace foundry_local {

    // ═══════════════════════════════════════════════════════════════════
    // JSON serialization helpers (responses-specific)
    // ═══════════════════════════════════════════════════════════════════

    namespace {

        // ── to_json ─────────────────────────────────────────────────

        nlohmann::json content_part_to_json(const ContentPart& cp) {
            nlohmann::json j;
            j["type"] = cp.type;
            if (cp.type == "refusal")
                j["refusal"] = cp.text;
            else if (cp.type == "input_image") {
                if (cp.image_url) j["image_url"] = *cp.image_url;
                if (cp.media_type) j["media_type"] = *cp.media_type;
                if (cp.detail) j["detail"] = *cp.detail;
            } else
                j["text"] = cp.text;
            return j;
        }

        nlohmann::json message_item_to_json(const ResponseMessageItem& m) {
            nlohmann::json j;
            j["type"] = m.type;
            j["role"] = m.role;
            if (!m.content_parts.empty()) {
                auto arr = nlohmann::json::array();
                for (const auto& cp : m.content_parts)
                    arr.push_back(content_part_to_json(cp));
                j["content"] = std::move(arr);
            } else {
                j["content"] = m.content_text;
            }
            if (m.id) j["id"] = *m.id;
            if (m.status) j["status"] = *m.status;
            return j;
        }

        nlohmann::json function_call_item_to_json(const ResponseFunctionCallItem& fc) {
            nlohmann::json j;
            j["type"] = fc.type;
            j["call_id"] = fc.call_id;
            j["name"] = fc.name;
            j["arguments"] = fc.arguments;
            if (fc.id) j["id"] = *fc.id;
            if (fc.status) j["status"] = *fc.status;
            return j;
        }

        nlohmann::json function_call_output_to_json(const ResponseFunctionCallOutputItem& fco) {
            nlohmann::json j;
            j["type"] = fco.type;
            j["call_id"] = fco.call_id;
            j["output"] = fco.output;
            if (fco.id) j["id"] = *fco.id;
            if (fco.status) j["status"] = *fco.status;
            return j;
        }

        nlohmann::json tool_def_to_json(const ResponseFunctionToolDefinition& t) {
            nlohmann::json j;
            j["type"] = t.type;
            j["name"] = t.name;
            if (t.description) j["description"] = *t.description;
            if (t.parameters) j["parameters"] = nlohmann::json::parse(*t.parameters);
            if (t.strict) j["strict"] = *t.strict;
            return j;
        }

        // ── from_json ───────────────────────────────────────────────

        ContentPart parse_content_part(const nlohmann::json& j) {
            ContentPart cp;
            cp.type = j.value("type", "");
            if (cp.type == "refusal")
                cp.text = j.value("refusal", "");
            else if (cp.type == "input_image") {
                if (j.contains("image_url") && j["image_url"].is_string())
                    cp.image_url = j["image_url"].get<std::string>();
                if (j.contains("media_type") && j["media_type"].is_string())
                    cp.media_type = j["media_type"].get<std::string>();
                if (j.contains("detail") && j["detail"].is_string())
                    cp.detail = j["detail"].get<std::string>();
            } else
                cp.text = j.value("text", "");
            return cp;
        }

        ResponseMessageItem parse_message_item(const nlohmann::json& j) {
            ResponseMessageItem m;
            m.type = j.value("type", "message");
            if (j.contains("id") && j["id"].is_string()) m.id = j["id"].get<std::string>();
            m.role = j.value("role", "");
            if (j.contains("content")) {
                const auto& c = j["content"];
                if (c.is_string()) {
                    m.content_text = c.get<std::string>();
                } else if (c.is_array()) {
                    for (const auto& p : c)
                        m.content_parts.push_back(parse_content_part(p));
                }
            }
            if (j.contains("status") && j["status"].is_string()) m.status = j["status"].get<std::string>();
            return m;
        }

        ResponseFunctionCallItem parse_function_call_item(const nlohmann::json& j) {
            ResponseFunctionCallItem fc;
            fc.type = j.value("type", "function_call");
            if (j.contains("id") && j["id"].is_string()) fc.id = j["id"].get<std::string>();
            fc.call_id = j.value("call_id", "");
            fc.name = j.value("name", "");
            if (j.contains("arguments")) {
                const auto& a = j["arguments"];
                fc.arguments = a.is_string() ? a.get<std::string>() : a.dump();
            }
            if (j.contains("status") && j["status"].is_string()) fc.status = j["status"].get<std::string>();
            return fc;
        }

        ResponseReasoningItem parse_reasoning_item(const nlohmann::json& j) {
            ResponseReasoningItem r;
            r.type = j.value("type", "reasoning");
            if (j.contains("id") && j["id"].is_string()) r.id = j["id"].get<std::string>();
            if (j.contains("content") && j["content"].is_array()) {
                for (const auto& p : j["content"])
                    r.content.push_back(parse_content_part(p));
            }
            if (j.contains("summary") && j["summary"].is_string()) r.summary = j["summary"].get<std::string>();
            if (j.contains("status") && j["status"].is_string()) r.status = j["status"].get<std::string>();
            return r;
        }

        ResponseOutputItem parse_output_item(const nlohmann::json& j) {
            ResponseOutputItem item;
            item.type = j.value("type", "");
            if (item.type == "message") {
                item.message = parse_message_item(j);
            } else if (item.type == "function_call") {
                item.function_call = parse_function_call_item(j);
            } else if (item.type == "reasoning") {
                item.reasoning = parse_reasoning_item(j);
            }
            return item;
        }

        ResponseFunctionToolDefinition parse_tool_def(const nlohmann::json& j) {
            ResponseFunctionToolDefinition t;
            t.type = j.value("type", "function");
            t.name = j.value("name", "");
            if (j.contains("description") && j["description"].is_string())
                t.description = j["description"].get<std::string>();
            if (j.contains("parameters") && j["parameters"].is_object())
                t.parameters = j["parameters"].dump();
            if (j.contains("strict") && j["strict"].is_boolean())
                t.strict = j["strict"].get<bool>();
            return t;
        }

        ResponseStatus parse_response_status(const std::string& s) {
            if (s == "queued") return ResponseStatus::Queued;
            if (s == "in_progress") return ResponseStatus::InProgress;
            if (s == "completed") return ResponseStatus::Completed;
            if (s == "failed") return ResponseStatus::Failed;
            if (s == "incomplete") return ResponseStatus::Incomplete;
            if (s == "cancelled") return ResponseStatus::Cancelled;
            return ResponseStatus::Queued;
        }

        ResponseUsage parse_usage(const nlohmann::json& j) {
            ResponseUsage u;
            u.input_tokens = j.value("input_tokens", 0);
            u.output_tokens = j.value("output_tokens", 0);
            u.total_tokens = j.value("total_tokens", 0);
            return u;
        }

        ResponseObject parse_response_object(const nlohmann::json& j) {
            ResponseObject r;
            r.id = j.value("id", "");
            r.object = j.value("object", "response");
            r.created_at = j.value("created_at", static_cast<int64_t>(0));
            r.status = parse_response_status(j.value("status", "queued"));
            r.model = j.value("model", "");

            if (j.contains("previous_response_id") && j["previous_response_id"].is_string())
                r.previous_response_id = j["previous_response_id"].get<std::string>();
            if (j.contains("instructions") && j["instructions"].is_string())
                r.instructions = j["instructions"].get<std::string>();

            if (j.contains("output") && j["output"].is_array()) {
                for (const auto& item : j["output"])
                    r.output.push_back(parse_output_item(item));
            }

            if (j.contains("error") && j["error"].is_object()) {
                ResponseError err;
                err.code = j["error"].value("code", "");
                err.message = j["error"].value("message", "");
                r.error = std::move(err);
            }

            if (j.contains("tools") && j["tools"].is_array()) {
                for (const auto& t : j["tools"])
                    r.tools.push_back(parse_tool_def(t));
            }

            if (j.contains("usage") && j["usage"].is_object())
                r.usage = parse_usage(j["usage"]);

            r.temperature = j.value("temperature", 1.0f);
            r.top_p = j.value("top_p", 1.0f);
            r.presence_penalty = j.value("presence_penalty", 0.0f);
            r.frequency_penalty = j.value("frequency_penalty", 0.0f);
            if (j.contains("max_output_tokens") && j["max_output_tokens"].is_number())
                r.max_output_tokens = j["max_output_tokens"].get<int>();
            if (j.contains("store") && j["store"].is_boolean())
                r.store = j["store"].get<bool>();

            return r;
        }

        StreamingEvent parse_streaming_event(const nlohmann::json& j) {
            StreamingEvent e;
            e.type = j.value("type", "");
            e.sequence_number = j.value("sequence_number", 0);

            // Lifecycle events
            if (j.contains("response") && j["response"].is_object())
                e.response = parse_response_object(j["response"]);

            // Output item events
            if (j.contains("item_id") && j["item_id"].is_string())
                e.item_id = j["item_id"].get<std::string>();
            if (j.contains("output_index") && j["output_index"].is_number())
                e.output_index = j["output_index"].get<int>();
            if (j.contains("item") && j["item"].is_object())
                e.item = parse_output_item(j["item"]);

            // Content part events
            if (j.contains("content_index") && j["content_index"].is_number())
                e.content_index = j["content_index"].get<int>();
            if (j.contains("part") && j["part"].is_object())
                e.part = parse_content_part(j["part"]);

            // Delta events
            if (j.contains("delta") && j["delta"].is_string())
                e.delta = j["delta"].get<std::string>();
            if (j.contains("text") && j["text"].is_string())
                e.text = j["text"].get<std::string>();
            if (j.contains("refusal") && j["refusal"].is_string())
                e.refusal = j["refusal"].get<std::string>();
            if (j.contains("arguments") && j["arguments"].is_string())
                e.arguments = j["arguments"].get<std::string>();
            if (j.contains("name") && j["name"].is_string())
                e.name = j["name"].get<std::string>();

            // Error events
            if (j.contains("code") && j["code"].is_string())
                e.code = j["code"].get<std::string>();
            if (j.contains("message") && j["message"].is_string())
                e.message = j["message"].get<std::string>();

            return e;
        }

    } // anonymous namespace

    // ═══════════════════════════════════════════════════════════════════
    // OpenAIResponsesClient implementation
    // ═══════════════════════════════════════════════════════════════════

    OpenAIResponsesClient::OpenAIResponsesClient(std::string baseUrl, std::string modelId,
                                                  gsl::not_null<ILogger*> logger)
        : baseUrl_(std::move(baseUrl)), modelId_(std::move(modelId)), logger_(logger) {
        // Strip trailing slashes
        while (!baseUrl_.empty() && baseUrl_.back() == '/')
            baseUrl_.pop_back();
        if (baseUrl_.empty()) {
            throw Exception("baseUrl must be a non-empty string.", *logger_);
        }
    }

    std::string OpenAIResponsesClient::SerializeInputItems(gsl::span<const ResponseInputItem> input) const {
        auto arr = nlohmann::json::array();
        for (const auto& item : input) {
            if (item.type == "message" && item.message)
                arr.push_back(message_item_to_json(*item.message));
            else if (item.type == "function_call" && item.function_call)
                arr.push_back(function_call_item_to_json(*item.function_call));
            else if (item.type == "function_call_output" && item.function_call_output)
                arr.push_back(function_call_output_to_json(*item.function_call_output));
            else if (item.type == "item_reference" && item.item_reference) {
                arr.push_back(nlohmann::json{{"type", "item_reference"}, {"id", item.item_reference->id}});
            }
        }
        return arr.dump();
    }

    std::string OpenAIResponsesClient::BuildRequestJson(
        const std::string& inputJson, bool stream,
        const std::optional<std::string>& previousResponseId,
        gsl::span<const ResponseFunctionToolDefinition> tools) const {

        nlohmann::json req;
        req["model"] = modelId_;
        req["input"] = nlohmann::json::parse(inputJson);
        req["stream"] = stream;

        // Merge settings
        if (settings_.instructions) req["instructions"] = *settings_.instructions;
        if (settings_.temperature) req["temperature"] = *settings_.temperature;
        if (settings_.top_p) req["top_p"] = *settings_.top_p;
        if (settings_.max_output_tokens) req["max_output_tokens"] = *settings_.max_output_tokens;
        if (settings_.frequency_penalty) req["frequency_penalty"] = *settings_.frequency_penalty;
        if (settings_.presence_penalty) req["presence_penalty"] = *settings_.presence_penalty;
        if (settings_.tool_choice) req["tool_choice"] = *settings_.tool_choice;
        if (settings_.truncation) req["truncation"] = *settings_.truncation;
        if (settings_.parallel_tool_calls) req["parallel_tool_calls"] = *settings_.parallel_tool_calls;
        if (settings_.store) req["store"] = *settings_.store;
        if (settings_.seed) req["seed"] = *settings_.seed;
        if (settings_.metadata) {
            nlohmann::json jMeta = nlohmann::json::object();
            for (const auto& [k, v] : *settings_.metadata)
                jMeta[k] = v;
            req["metadata"] = std::move(jMeta);
        }
        if (settings_.reasoning) {
            nlohmann::json jReasoning = nlohmann::json::object();
            if (settings_.reasoning->effort) jReasoning["effort"] = *settings_.reasoning->effort;
            if (settings_.reasoning->summary) jReasoning["summary"] = *settings_.reasoning->summary;
            req["reasoning"] = std::move(jReasoning);
        }
        if (settings_.text) {
            nlohmann::json jText = nlohmann::json::object();
            if (settings_.text->format) {
                nlohmann::json jFmt = {{"type", settings_.text->format->type}};
                if (settings_.text->format->name) jFmt["name"] = *settings_.text->format->name;
                if (settings_.text->format->description) jFmt["description"] = *settings_.text->format->description;
                if (settings_.text->format->schema) jFmt["schema"] = nlohmann::json::parse(*settings_.text->format->schema);
                if (settings_.text->format->strict) jFmt["strict"] = *settings_.text->format->strict;
                jText["format"] = std::move(jFmt);
            }
            if (settings_.text->verbosity) jText["verbosity"] = *settings_.text->verbosity;
            req["text"] = std::move(jText);
        }

        if (previousResponseId) req["previous_response_id"] = *previousResponseId;

        if (!tools.empty()) {
            auto jTools = nlohmann::json::array();
            for (const auto& t : tools)
                jTools.push_back(tool_def_to_json(t));
            req["tools"] = std::move(jTools);
        }

        return req.dump();
    }

    // ── Non-streaming ────────────────────────────────────────────────

    ResponseObject OpenAIResponsesClient::Create(
        const std::string& input,
        const std::optional<std::string>& previousResponseId,
        gsl::span<const ResponseFunctionToolDefinition> tools) const {

        std::string body = BuildRequestJson(nlohmann::json(input).dump(), false, previousResponseId, tools);
        std::string url = baseUrl_ + "/v1/responses";

        logger_->Log(LogLevel::Debug, "POST " + url);

        auto resp = detail::WinHttpRequest("POST", url, body);
        if (resp.statusCode < 200 || resp.statusCode >= 300) {
            throw Exception("Responses API error (" + std::to_string(resp.statusCode) + "): " + resp.body, *logger_);
        }

        return parse_response_object(nlohmann::json::parse(resp.body));
    }

    ResponseObject OpenAIResponsesClient::Create(
        gsl::span<const ResponseInputItem> input,
        const std::optional<std::string>& previousResponseId,
        gsl::span<const ResponseFunctionToolDefinition> tools) const {

        auto inputJson = SerializeInputItems(input);
        std::string body = BuildRequestJson(inputJson, false, previousResponseId, tools);
        std::string url = baseUrl_ + "/v1/responses";

        logger_->Log(LogLevel::Debug, "POST " + url);

        auto resp = detail::WinHttpRequest("POST", url, body);
        if (resp.statusCode < 200 || resp.statusCode >= 300) {
            throw Exception("Responses API error (" + std::to_string(resp.statusCode) + "): " + resp.body, *logger_);
        }

        return parse_response_object(nlohmann::json::parse(resp.body));
    }

    // ── Streaming ────────────────────────────────────────────────────

    namespace {
        auto makeSSEHandler(const OpenAIResponsesClient::StreamCallback& onEvent, ILogger& logger) {
            return [&onEvent, &logger](const std::string& eventData) {
                try {
                    auto parsed = parse_streaming_event(nlohmann::json::parse(eventData));
                    onEvent(parsed);
                } catch (const std::exception& e) {
                    logger.Log(LogLevel::Warning,
                               std::string("Failed to parse streaming event: ") + e.what());
                }
            };
        }
    }

    void OpenAIResponsesClient::CreateStreaming(
        const std::string& input, const StreamCallback& onEvent,
        const std::optional<std::string>& previousResponseId,
        gsl::span<const ResponseFunctionToolDefinition> tools) const {

        std::string body = BuildRequestJson(nlohmann::json(input).dump(), true, previousResponseId, tools);
        std::string url = baseUrl_ + "/v1/responses";

        logger_->Log(LogLevel::Debug, "POST (streaming) " + url);
        detail::WinHttpStreamSSE(url, body, makeSSEHandler(onEvent, *logger_));
    }

    void OpenAIResponsesClient::CreateStreaming(
        gsl::span<const ResponseInputItem> input, const StreamCallback& onEvent,
        const std::optional<std::string>& previousResponseId,
        gsl::span<const ResponseFunctionToolDefinition> tools) const {

        auto inputJson = SerializeInputItems(input);
        std::string body = BuildRequestJson(inputJson, true, previousResponseId, tools);
        std::string url = baseUrl_ + "/v1/responses";

        logger_->Log(LogLevel::Debug, "POST (streaming) " + url);
        detail::WinHttpStreamSSE(url, body, makeSSEHandler(onEvent, *logger_));
    }

    // ── CRUD ─────────────────────────────────────────────────────────

    ResponseObject OpenAIResponsesClient::Get(const std::string& responseId) const {
        std::string url = baseUrl_ + "/v1/responses/" + responseId;
        logger_->Log(LogLevel::Debug, "GET " + url);

        auto resp = detail::WinHttpRequest("GET", url);
        if (resp.statusCode < 200 || resp.statusCode >= 300) {
            throw Exception("Responses API GET error (" + std::to_string(resp.statusCode) + "): " + resp.body, *logger_);
        }
        return parse_response_object(nlohmann::json::parse(resp.body));
    }

    DeleteResponseResult OpenAIResponsesClient::Delete(const std::string& responseId) const {
        std::string url = baseUrl_ + "/v1/responses/" + responseId;
        logger_->Log(LogLevel::Debug, "DELETE " + url);

        auto resp = detail::WinHttpRequest("DELETE", url);
        if (resp.statusCode < 200 || resp.statusCode >= 300) {
            throw Exception("Responses API DELETE error (" + std::to_string(resp.statusCode) + "): " + resp.body, *logger_);
        }
        auto j = nlohmann::json::parse(resp.body);
        DeleteResponseResult r;
        r.id = j.value("id", "");
        r.object = j.value("object", "");
        r.deleted = j.value("deleted", false);
        return r;
    }

    ResponseObject OpenAIResponsesClient::Cancel(const std::string& responseId) const {
        std::string url = baseUrl_ + "/v1/responses/" + responseId + "/cancel";
        logger_->Log(LogLevel::Debug, "POST " + url);

        auto resp = detail::WinHttpRequest("POST", url);
        if (resp.statusCode < 200 || resp.statusCode >= 300) {
            throw Exception("Responses API cancel error (" + std::to_string(resp.statusCode) + "): " + resp.body, *logger_);
        }
        return parse_response_object(nlohmann::json::parse(resp.body));
    }

    InputItemsListResponse OpenAIResponsesClient::GetInputItems(const std::string& responseId) const {
        std::string url = baseUrl_ + "/v1/responses/" + responseId + "/input_items";
        logger_->Log(LogLevel::Debug, "GET " + url);

        auto resp = detail::WinHttpRequest("GET", url);
        if (resp.statusCode < 200 || resp.statusCode >= 300) {
            throw Exception("Responses API input_items error (" + std::to_string(resp.statusCode) + "): " + resp.body, *logger_);
        }
        auto j = nlohmann::json::parse(resp.body);
        InputItemsListResponse result;
        result.object = j.value("object", "list");
        // Note: parsing input items from GET is left generic for now
        return result;
    }

} // namespace foundry_local
