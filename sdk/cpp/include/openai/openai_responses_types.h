// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <cstdint>

namespace foundry_local {

    // ── Response Status ─────────────────────────────────────────────────

    enum class ResponseStatus {
        Queued, InProgress, Completed, Failed, Incomplete, Cancelled
    };

    // ── Content Part ────────────────────────────────────────────────────

    /// Represents a content part: input_text, output_text, refusal, or input_image.
    struct ContentPart {
        std::string type;  ///< "input_text", "output_text", "refusal", "input_image"
        std::string text;  ///< Text content (or refusal text when type == "refusal")

        // Image fields (only when type == "input_image"):
        std::optional<std::string> image_url;
        std::optional<std::string> media_type;
        std::optional<std::string> detail;  ///< "auto", "low", "high"
    };

    // ── Tool Definition (Responses API — flattened) ─────────────────────

    struct ResponseFunctionToolDefinition {
        std::string type = "function";
        std::string name;
        std::optional<std::string> description;
        std::optional<std::string> parameters;  ///< JSON string
        std::optional<bool> strict;
    };

    // ── Response Items ──────────────────────────────────────────────────

    struct ResponseMessageItem {
        std::string type = "message";
        std::optional<std::string> id;
        std::string role;
        std::string content_text;
        std::vector<ContentPart> content_parts;
        std::optional<std::string> status;
    };

    struct ResponseFunctionCallItem {
        std::string type = "function_call";
        std::optional<std::string> id;
        std::string call_id;
        std::string name;
        std::string arguments;
        std::optional<std::string> status;
    };

    struct ResponseFunctionCallOutputItem {
        std::string type = "function_call_output";
        std::optional<std::string> id;
        std::string call_id;
        std::string output;
        std::optional<std::string> status;
    };

    struct ResponseItemReference {
        std::string type = "item_reference";
        std::string id;
    };

    struct ResponseReasoningItem {
        std::string type = "reasoning";
        std::optional<std::string> id;
        std::vector<ContentPart> content;
        std::optional<std::string> summary;
        std::optional<std::string> status;
    };

    /// Holds any output item (message | function_call | reasoning).
    struct ResponseOutputItem {
        std::string type;
        std::optional<ResponseMessageItem> message;
        std::optional<ResponseFunctionCallItem> function_call;
        std::optional<ResponseReasoningItem> reasoning;
    };

    /// Holds any input item.
    struct ResponseInputItem {
        std::string type;
        std::optional<ResponseMessageItem> message;
        std::optional<ResponseFunctionCallItem> function_call;
        std::optional<ResponseFunctionCallOutputItem> function_call_output;
        std::optional<ResponseItemReference> item_reference;
        std::optional<ResponseReasoningItem> reasoning;
    };

    // ── Response Object ─────────────────────────────────────────────────

    struct ResponseUsage {
        int input_tokens = 0;
        int output_tokens = 0;
        int total_tokens = 0;
    };

    struct ResponseError {
        std::string code;
        std::string message;
    };

    struct ResponseObject {
        std::string id;
        std::string object = "response";
        int64_t created_at = 0;
        ResponseStatus status = ResponseStatus::Queued;
        std::string model;
        std::optional<std::string> previous_response_id;
        std::optional<std::string> instructions;
        std::vector<ResponseOutputItem> output;
        std::optional<ResponseError> error;
        std::vector<ResponseFunctionToolDefinition> tools;
        std::optional<ResponseUsage> usage;
        std::optional<bool> store;
        float temperature = 1.0f;
        float top_p = 1.0f;
        float presence_penalty = 0.0f;
        float frequency_penalty = 0.0f;
        std::optional<int> max_output_tokens;
    };

    struct DeleteResponseResult {
        std::string id;
        std::string object;
        bool deleted = false;
    };

    struct InputItemsListResponse {
        std::string object = "list";
        std::vector<ResponseInputItem> data;
    };

    // ── Streaming Event ─────────────────────────────────────────────────

    struct StreamingEvent {
        std::string type;
        int sequence_number = 0;
        std::optional<ResponseObject> response;
        std::optional<std::string> item_id;
        std::optional<int> output_index;
        std::optional<ResponseOutputItem> item;
        std::optional<int> content_index;
        std::optional<ContentPart> part;
        std::optional<std::string> delta;
        std::optional<std::string> text;
        std::optional<std::string> refusal;
        std::optional<std::string> arguments;
        std::optional<std::string> name;
        std::optional<std::string> code;
        std::optional<std::string> message;
    };

    // ── Settings ────────────────────────────────────────────────────────

    struct ReasoningConfig {
        std::optional<std::string> effort;
        std::optional<std::string> summary;
    };

    struct TextFormat {
        std::string type;
        std::optional<std::string> name;
        std::optional<std::string> description;
        std::optional<std::string> schema;
        std::optional<bool> strict;
    };

    struct TextConfig {
        std::optional<TextFormat> format;
        std::optional<std::string> verbosity;
    };

    struct ResponseSettings {
        std::optional<std::string> instructions;
        std::optional<float> temperature;
        std::optional<float> top_p;
        std::optional<int> max_output_tokens;
        std::optional<float> frequency_penalty;
        std::optional<float> presence_penalty;
        std::optional<std::string> tool_choice;
        std::optional<std::string> truncation;
        std::optional<bool> parallel_tool_calls;
        std::optional<bool> store;
        std::optional<std::unordered_map<std::string, std::string>> metadata;
        std::optional<ReasoningConfig> reasoning;
        std::optional<TextConfig> text;
        std::optional<int> seed;
    };

    // ── Helpers ─────────────────────────────────────────────────────────

    /// Extracts text from the first assistant message.
    inline std::string GetOutputText(const ResponseObject& response) {
        for (const auto& item : response.output) {
            if (item.type == "message" && item.message && item.message->role == "assistant") {
                if (!item.message->content_parts.empty()) {
                    std::string result;
                    for (const auto& p : item.message->content_parts)
                        if (p.type == "output_text") result += p.text;
                    return result;
                }
                return item.message->content_text;
            }
        }
        return {};
    }

} // namespace foundry_local
