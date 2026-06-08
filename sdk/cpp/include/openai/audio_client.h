// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string>
#include <string_view>
#include <functional>
#include <filesystem>
#include <memory>

#include <gsl/pointers>

namespace foundry_local::Internal {
    struct IFoundryLocalCore;
}

namespace foundry_local {
    class ILogger;
    class IModel;

    struct AudioCreateTranscriptionResponse {
        std::string text;
    };

    class LiveAudioTranscriptionSession;

    class OpenAIAudioClient final {
    public:
        explicit OpenAIAudioClient(const IModel& model);

        /// Returns the model ID this client was created for.
        const std::string& GetModelId() const noexcept { return modelId_; }

        AudioCreateTranscriptionResponse TranscribeAudio(const std::filesystem::path& audioFilePath) const;

        using StreamCallback = std::function<void(const AudioCreateTranscriptionResponse& chunk)>;
        void TranscribeAudioStreaming(const std::filesystem::path& audioFilePath, const StreamCallback& onChunk) const;

        /// Create a new live audio transcription session for streaming PCM audio.
        std::unique_ptr<LiveAudioTranscriptionSession> CreateLiveTranscriptionSession() const;

    private:
        OpenAIAudioClient(gsl::not_null<foundry_local::Internal::IFoundryLocalCore*> core, std::string_view modelId,
                          gsl::not_null<ILogger*> logger);

        std::string modelId_;
        gsl::not_null<foundry_local::Internal::IFoundryLocalCore*> core_;
        gsl::not_null<ILogger*> logger_;
    };

} // namespace foundry_local
