// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <string>
#include <string_view>
#include <filesystem>
#include <cstdint>

#include <gsl/span>
#include <nlohmann/json.hpp>

#include "foundry_local.h"
#include "foundry_local_internal_core.h"
#include "foundry_local_exception.h"
#include "core_interop_request.h"
#include "core_helpers.h"
#include "logger.h"

#include "openai/openai_live_audio_client.h"

namespace foundry_local {

    OpenAIAudioClient::OpenAIAudioClient(gsl::not_null<Internal::IFoundryLocalCore*> core, std::string_view modelId,
                                         gsl::not_null<ILogger*> logger)
        : core_(core), modelId_(modelId), logger_(logger) {}

    AudioCreateTranscriptionResponse OpenAIAudioClient::TranscribeAudio(
        const std::filesystem::path& audioFilePath) const {
        nlohmann::json openAiReq = {{"Model", modelId_}, {"FileName", audioFilePath.string()}};
        CoreInteropRequest req("audio_transcribe");
        req.AddParam("OpenAICreateRequest", openAiReq.dump());

        std::string json = req.ToJson();

        auto coreResponse = core_->call(req.Command(), *logger_, &json);
        if (coreResponse.HasError()) {
            throw Exception("Audio transcription failed: " + coreResponse.error, *logger_);
        }

        AudioCreateTranscriptionResponse response;
        response.text = std::move(coreResponse.data);

        return response;
    }

    void OpenAIAudioClient::TranscribeAudioStreaming(const std::filesystem::path& audioFilePath,
                                                     const StreamCallback& onChunk) const {
        nlohmann::json openAiReq = {{"Model", modelId_}, {"FileName", audioFilePath.string()}};
        CoreInteropRequest req("audio_transcribe");
        req.AddParam("OpenAICreateRequest", openAiReq.dump());

        std::string json = req.ToJson();

        detail::CallWithStreamingCallback(
            core_, req.Command(), json, *logger_,
            [&onChunk](const std::string& text) {
                AudioCreateTranscriptionResponse chunk;
                chunk.text = text;
                onChunk(chunk);
            },
            "Streaming audio transcription failed: ");
    }

    OpenAIAudioClient::OpenAIAudioClient(const IModel& model)
        : OpenAIAudioClient(model.GetCoreAccess().core, model.GetCoreAccess().modelName, model.GetCoreAccess().logger) {
        if (!model.IsLoaded()) {
            throw Exception("Model " + model.GetCoreAccess().modelName + " is not loaded. Call Load() first.",
                            *model.GetCoreAccess().logger);
        }
    }

    std::unique_ptr<LiveAudioTranscriptionSession> OpenAIAudioClient::CreateLiveTranscriptionSession() const {
        return std::make_unique<LiveAudioTranscriptionSession>(core_, modelId_, logger_);
    }

} // namespace foundry_local
