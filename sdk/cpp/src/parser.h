// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include <string>
#include <vector>
#include "foundry_local.h"
#include <nlohmann/json.hpp>

namespace FoundryLocal {
    inline DeviceType parse_device_type(std::string_view v) {
        if (v == "CPU") {
            return DeviceType::CPU;
        }
        if (v == "NPU") {
            return DeviceType::NPU;
        }
        if (v == "GPU") {
            return DeviceType::GPU;
        }
        return DeviceType::Invalid;
    }

    inline FinishReason parse_finish_reason(std::string_view v) {
        if (v == "stop")
            return FinishReason::Stop;
        if (v == "length")
            return FinishReason::Length;
        if (v == "tool_calls")
            return FinishReason::ToolCalls;
        if (v == "content_filter")
            return FinishReason::ContentFilter;
        return FinishReason::None;
    }

    // ---------- Helpers ----------
    inline std::string get_string_or_empty(const nlohmann::json& j, const char* key) {
        auto it = j.find(key);
        std::string out = "";
        if (it != j.end() && it->is_string()) {
            out = it->get<std::string>();
        }
        return out;
    }

    inline void from_json(const nlohmann::json& j, Runtime& r) {
        std::string deviceType;
        std::string executionProvider;
        j.at("deviceType").get_to(deviceType);
        j.at("executionProvider").get_to(r.execution_provider);

        r.device_type = parse_device_type(std::move(deviceType));
    }

    inline void from_json(const nlohmann::json& j, PromptTemplate& p) {
        p.system = get_string_or_empty(j, "system");
        p.user = get_string_or_empty(j, "user");
        p.assistant = get_string_or_empty(j, "assistant");
        p.prompt = get_string_or_empty(j, "prompt");
    }

    inline std::optional<std::string> get_opt_string(const nlohmann::json& j, const char* key) {
        auto it = j.find(key);
        if (it == j.end() || it->is_null()) {
            return std::nullopt;
        }
        if (it->is_string()) {
            return it->get<std::string>();
        }
        return std::nullopt;
    }

    inline std::optional<int> get_opt_int(const nlohmann::json& j, const char* key) {
        auto it = j.find(key);
        if (it == j.end() || it->is_null()) {
            return std::nullopt;
        }
        if (it->is_number_integer()) {
            return it->get<int>();
        }
        return std::nullopt;
    }

    inline std::optional<int64_t> get_opt_i64(const nlohmann::json& j, const char* key) {
        auto it = j.find(key);
        if (it == j.end() || it->is_null()) {
            return std::nullopt;
        }
        if (it->is_number_integer()) {
            return it->get<int64_t>();
        }
        return std::nullopt;
    }

    inline std::optional<bool> get_opt_bool(const nlohmann::json& j, const char* key) {
        auto it = j.find(key);
        if (it == j.end() || it->is_null()) {
            return std::nullopt;
        }
        if (it->is_boolean()) {
            return it->get<bool>();
        }
        return std::nullopt;
    }

    inline void from_json(const nlohmann::json& j, Parameter& p) {
        j.at("name").get_to(p.name);
        p.value = get_opt_string(j, "value");
    }

    inline void from_json(const nlohmann::json& j, ModelSettings& ms) {
        ms.parameters.clear();
        if (auto it = j.find("parameters"); it != j.end() && it->is_array()) {
            ms.parameters = it->get<std::vector<Parameter>>();
        }
    }

    inline void from_json(const nlohmann::json& j, ModelInfo& m) {
        j.at("id").get_to(m.id);
        j.at("name").get_to(m.name);
        j.at("version").get_to(m.version);
        j.at("alias").get_to(m.alias);
        j.at("providerType").get_to(m.provider_type);
        j.at("uri").get_to(m.uri);
        j.at("modelType").get_to(m.model_type);

        m.display_name = get_opt_string(j, "displayName");
        m.publisher = get_opt_string(j, "publisher");
        m.license = get_opt_string(j, "license");
        m.license_description = get_opt_string(j, "licenseDescription");
        m.task = get_opt_string(j, "task");
        if (auto it = j.find("fileSizeMb"); it != j.end() && !it->is_null() && it->is_number_integer()) {
            auto v = it->get<int64_t>();
            m.file_size_mb = (v >= 0) ? static_cast<uint32_t>(v) : 0u;
        }
        m.supports_tool_calling = get_opt_bool(j, "supportsToolCalling");
        m.max_output_tokens = get_opt_i64(j, "maxOutputTokens");
        m.min_fl_version = get_opt_string(j, "minFLVersion");

        if (auto it = j.find("cached"); it != j.end() && it->is_boolean()) {
            m.cached = it->get<bool>();
        }
        else {
            m.cached = false;
        }

        if (auto it = j.find("createdAt"); it != j.end() && it->is_number_integer()) {
            m.created_at_unix = it->get<int64_t>();
        }
        else {
            m.created_at_unix = 0;
        }

        // nested optional objects
        if (auto it = j.find("modelSettings"); it != j.end() && it->is_object()) {
            m.model_settings = it->get<ModelSettings>();
        }
        else {
            m.model_settings.reset();
        }

        if (auto it = j.find("promptTemplate"); it != j.end() && it->is_object()) {
            m.prompt_template = it->get<PromptTemplate>();
        }
        else {
            m.prompt_template.reset();
        }

        if (auto it = j.find("runtime"); it != j.end() && it->is_object()) {
            m.runtime = it->get<Runtime>();
        }
        else {
            m.runtime.reset();
        }
    }

    // ---------- Tool calling: to_json (serialization for requests) ----------

    inline void to_json(nlohmann::json& j, const PropertyDefinition& pd) {
        j = nlohmann::json{{"type", pd.type}};
        if (pd.description)
            j["description"] = *pd.description;
        if (pd.properties) {
            nlohmann::json props = nlohmann::json::object();
            for (const auto& [key, val] : *pd.properties) {
                nlohmann::json pj;
                to_json(pj, val);
                props[key] = std::move(pj);
            }
            j["properties"] = std::move(props);
        }
        if (pd.required)
            j["required"] = *pd.required;
    }

    inline void to_json(nlohmann::json& j, const FunctionDefinition& fd) {
        j = nlohmann::json{{"name", fd.name}};
        if (fd.description)
            j["description"] = *fd.description;
        if (fd.parameters) {
            nlohmann::json pj;
            to_json(pj, *fd.parameters);
            j["parameters"] = std::move(pj);
        }
    }

    inline void to_json(nlohmann::json& j, const ToolDefinition& td) {
        j = nlohmann::json{{"type", td.type}};
        nlohmann::json fj;
        to_json(fj, td.function);
        j["function"] = std::move(fj);
    }

    // ---------- Tool calling: from_json (deserialization from responses) ----------

    inline void from_json(const nlohmann::json& j, FunctionCall& fc) {
        fc.name = get_string_or_empty(j, "name");
        if (j.contains("arguments")) {
            const auto& args = j.at("arguments");
            if (args.is_string())
                fc.arguments = args.get<std::string>();
            else
                fc.arguments = args.dump();
        }
    }

    inline void from_json(const nlohmann::json& j, ToolCall& tc) {
        tc.id = get_string_or_empty(j, "id");
        tc.type = get_string_or_empty(j, "type");
        if (j.contains("function") && j.at("function").is_object())
            tc.function_call = j.at("function").get<FunctionCall>();
    }

    inline void from_json(const nlohmann::json& j, ChatMessage& m) {
        if (j.contains("role"))
            j.at("role").get_to(m.role);
        if (j.contains("content") && !j.at("content").is_null())
            j.at("content").get_to(m.content);

        m.tool_call_id = get_opt_string(j, "tool_call_id");

        m.tool_calls.clear();
        if (j.contains("tool_calls") && j.at("tool_calls").is_array()) {
            for (const auto& tc : j.at("tool_calls")) {
                if (tc.is_object())
                    m.tool_calls.push_back(tc.get<ToolCall>());
            }
        }
    }

    inline void from_json(const nlohmann::json& j, ChatChoice& c) {
        if (j.contains("index"))
            j.at("index").get_to(c.index);
        if (j.contains("finish_reason") && !j.at("finish_reason").is_null())
            c.finish_reason = parse_finish_reason(j.at("finish_reason").get<std::string_view>());

        if (j.contains("message") && !j.at("message").is_null())
            c.message = j.at("message").get<ChatMessage>();

        if (j.contains("delta") && !j.at("delta").is_null())
            c.delta = j.at("delta").get<ChatMessage>();
    }

    inline void from_json(const nlohmann::json& j, ChatCompletionCreateResponse& r) {
        if (j.contains("created"))
            j.at("created").get_to(r.created);
        r.id = get_string_or_empty(j, "id");
        if (j.contains("IsDelta"))
            j.at("IsDelta").get_to(r.is_delta);
        if (j.contains("Successful"))
            j.at("Successful").get_to(r.successful);
        if (j.contains("HttpStatusCode"))
            j.at("HttpStatusCode").get_to(r.http_status_code);

        r.choices.clear();
        if (j.contains("choices") && j.at("choices").is_array()) {
            r.choices = j.at("choices").get<std::vector<ChatChoice>>();
        }
    }

    // ---------- Tool choice helpers ----------

    inline std::string tool_choice_to_string(ToolChoiceKind kind) {
        switch (kind) {
            case ToolChoiceKind::Auto: return "auto";
            case ToolChoiceKind::None: return "none";
            case ToolChoiceKind::Required: return "required";
        }
        return "auto";
    }

} // namespace FoundryLocal