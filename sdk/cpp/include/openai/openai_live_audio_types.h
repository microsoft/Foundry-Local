// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string>
#include <vector>
#include <optional>

namespace foundry_local {

    struct ContentPart {
        std::string text;
        std::string transcript;
    };

    struct LiveAudioTranscriptionResponse {
        std::string text;
        bool is_final = false;
        std::optional<double> start_time;
        std::optional<double> end_time;
        std::vector<ContentPart> content;

        static LiveAudioTranscriptionResponse FromJson(const std::string& json);
    };

    struct LiveAudioTranscriptionOptions {
        int sample_rate = 16000;
        int channels = 1;
        int bits_per_sample = 16;
        std::optional<std::string> language;
        int push_queue_capacity = 100;
    };

    struct CoreErrorResponse {
        std::string code;
        std::string message;
        bool is_transient = false;

        static std::optional<CoreErrorResponse> TryParse(const std::string& error_string);
    };

    enum class TranscriptionStatus {
        Result,
        Timeout,
        Closed,
        Error
    };

} // namespace foundry_local
