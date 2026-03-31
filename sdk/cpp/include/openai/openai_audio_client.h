// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string>
#include <string_view>
#include <functional>
#include <filesystem>

#include <gsl/pointers>

namespace FoundryLocal::Internal {
    struct IFoundryLocalCore;
}

namespace FoundryLocal {
class ILogger;
class IModel;

    struct AudioCreateTranscriptionResponse {
        std::string text;
    };

    class OpenAIAudioClient final {
    public:
        explicit OpenAIAudioClient(const IModel& model);

        /// Returns the model ID this client was created for.
        const std::string& GetModelId() const noexcept { return modelId_; }

        AudioCreateTranscriptionResponse TranscribeAudio(const std::filesystem::path& audioFilePath) const;

        using StreamCallback = std::function<void(const AudioCreateTranscriptionResponse& chunk)>;
        void TranscribeAudioStreaming(const std::filesystem::path& audioFilePath, const StreamCallback& onChunk) const;

    private:
        OpenAIAudioClient(gsl::not_null<FoundryLocal::Internal::IFoundryLocalCore*> core, std::string_view modelId,
                    gsl::not_null<ILogger*> logger);

        std::string modelId_;
        gsl::not_null<FoundryLocal::Internal::IFoundryLocalCore*> core_;
        gsl::not_null<ILogger*> logger_;
    };

    /// Backward-compatible alias.
    using AudioClient = OpenAIAudioClient;

} // namespace FoundryLocal
