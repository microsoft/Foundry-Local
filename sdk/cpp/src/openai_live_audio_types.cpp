// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <string>
#include <optional>

#include <nlohmann/json.hpp>

#include "openai/openai_live_audio_types.h"

namespace foundry_local {

    LiveAudioTranscriptionResponse LiveAudioTranscriptionResponse::FromJson(const std::string& json) {
        auto j = nlohmann::json::parse(json);
        LiveAudioTranscriptionResponse response;

        if (j.contains("text") && j["text"].is_string()) {
            response.text = j["text"].get<std::string>();
        }

        if (j.contains("is_final") && j["is_final"].is_boolean()) {
            response.is_final = j["is_final"].get<bool>();
        }
        else if (j.contains("isFinal") && j["isFinal"].is_boolean()) {
            response.is_final = j["isFinal"].get<bool>();
        }

        if (j.contains("start_time") && j["start_time"].is_number()) {
            response.start_time = j["start_time"].get<double>();
        }
        else if (j.contains("startTime") && j["startTime"].is_number()) {
            response.start_time = j["startTime"].get<double>();
        }

        if (j.contains("end_time") && j["end_time"].is_number()) {
            response.end_time = j["end_time"].get<double>();
        }
        else if (j.contains("endTime") && j["endTime"].is_number()) {
            response.end_time = j["endTime"].get<double>();
        }

        if (j.contains("content") && j["content"].is_array()) {
            for (const auto& item : j["content"]) {
                ContentPart part;
                if (item.contains("text") && item["text"].is_string()) {
                    part.text = item["text"].get<std::string>();
                }
                if (item.contains("transcript") && item["transcript"].is_string()) {
                    part.transcript = item["transcript"].get<std::string>();
                }
                response.content.push_back(std::move(part));
            }
        }

        return response;
    }

    std::optional<CoreErrorResponse> CoreErrorResponse::TryParse(const std::string& error_string) {
        try {
            auto j = nlohmann::json::parse(error_string);
            CoreErrorResponse response;

            if (j.contains("code") && j["code"].is_string()) {
                response.code = j["code"].get<std::string>();
            }
            if (j.contains("message") && j["message"].is_string()) {
                response.message = j["message"].get<std::string>();
            }
            if (j.contains("is_transient") && j["is_transient"].is_boolean()) {
                response.is_transient = j["is_transient"].get<bool>();
            }
            else if (j.contains("isTransient") && j["isTransient"].is_boolean()) {
                response.is_transient = j["isTransient"].get<bool>();
            }

            return response;
        }
        catch (const nlohmann::json::exception&) {
            return std::nullopt;
        }
    }

} // namespace foundry_local
