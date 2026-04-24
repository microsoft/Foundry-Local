// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <cstdint>

#include <nlohmann/json.hpp>

#include "openai/openai_live_audio_client.h"
#include "openai/openai_live_audio_types.h"
#include "foundry_local_internal_core.h"
#include "foundry_local_exception.h"
#include "core_interop_request.h"
#include "thread_safe_queue.h"
#include "logger.h"

namespace foundry_local {

    LiveAudioTranscriptionSession::LiveAudioTranscriptionSession(
        gsl::not_null<Internal::IFoundryLocalCore*> core,
        std::string modelId,
        gsl::not_null<ILogger*> logger)
        : core_(core), modelId_(std::move(modelId)), logger_(logger) {}

    LiveAudioTranscriptionSession::~LiveAudioTranscriptionSession() noexcept {
        try {
            std::unique_lock<std::mutex> lock(mutex_);
            if (state_ == SessionState::Started) {
                StopInternal(lock);
            }
        }
        catch (...) {
            // Suppress exceptions in destructor
        }
    }

    void LiveAudioTranscriptionSession::Start() {
        std::unique_lock<std::mutex> lock(mutex_);
        if (state_ != SessionState::Created) {
            throw Exception("Session has already been started.", *logger_);
        }

        // Transition to Starting state before releasing lock for FFI call
        state_ = SessionState::Starting;
        activeSettings_ = settings_;

        // Build the start command
        CoreInteropRequest req("audio_stream_start");
        req.AddParam("Model", modelId_);
        req.AddParam("SampleRate", std::to_string(activeSettings_.sample_rate));
        req.AddParam("Channels", std::to_string(activeSettings_.channels));
        req.AddParam("BitsPerSample", std::to_string(activeSettings_.bits_per_sample));
        if (activeSettings_.language.has_value()) {
            req.AddParam("Language", activeSettings_.language.value());
        }
        std::string json = req.ToJson();

        // Release lock during FFI call to avoid holding mutex across boundary
        lock.unlock();

        auto response = core_->call(req.Command(), *logger_, &json);

        lock.lock();

        if (response.HasError()) {
            state_ = SessionState::Created;
            throw Exception("Failed to start audio stream: " + response.error, *logger_);
        }

        sessionHandle_ = std::move(response.data);
        if (sessionHandle_.empty()) {
            state_ = SessionState::Created;
            throw Exception("audio_stream_start returned an empty session handle.", *logger_);
        }

        // Create the queues
        pushQueue_ = std::make_unique<Internal::ThreadSafeQueue<AudioChunk>>(
            static_cast<size_t>(activeSettings_.push_queue_capacity));
        resultQueue_ = std::make_unique<Internal::ThreadSafeQueue<LiveAudioTranscriptionResponse>>(
            static_cast<size_t>(activeSettings_.push_queue_capacity));

        state_ = SessionState::Started;

        // Start the push worker thread
        pushThread_ = std::thread([this] { PushWorkerLoop(); });
    }

    void LiveAudioTranscriptionSession::Append(const uint8_t* pcmData, size_t length) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (state_ != SessionState::Started) {
                throw Exception("Session is not started. Call Start() first.", *logger_);
            }
        }

        AudioChunk chunk(pcmData, pcmData + length);
        if (!pushQueue_->Push(std::move(chunk))) {
            throw Exception("Failed to enqueue audio data: session is closed.", *logger_);
        }
    }

    TranscriptionStatus LiveAudioTranscriptionSession::TryGetNext(LiveAudioTranscriptionResponse& result,
                                                                   std::chrono::milliseconds timeout) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (state_ != SessionState::Started && state_ != SessionState::Stopped) {
                throw Exception("Session is not started. Call Start() first.", *logger_);
            }
        }

        auto status = resultQueue_->TryPop(result, timeout);
        switch (status) {
            case Internal::DequeueStatus::Item:
                return TranscriptionStatus::Result;
            case Internal::DequeueStatus::Timeout:
                return TranscriptionStatus::Timeout;
            case Internal::DequeueStatus::Closed:
                return TranscriptionStatus::Closed;
            case Internal::DequeueStatus::Error:
                return TranscriptionStatus::Error;
            default:
                return TranscriptionStatus::Error;
        }
    }

    void LiveAudioTranscriptionSession::Stop() {
        std::unique_lock<std::mutex> lock(mutex_);
        if (state_ != SessionState::Started) {
            return;
        }
        StopInternal(lock);
    }

    void LiveAudioTranscriptionSession::StopInternal(std::unique_lock<std::mutex>& lock) {
        state_ = SessionState::Stopped;
        std::string handle = sessionHandle_;

        // Close the push queue to signal the worker thread to finish
        if (pushQueue_) {
            pushQueue_->Close();
        }

        lock.unlock();

        // Wait for the push thread to finish
        if (pushThread_.joinable()) {
            pushThread_.join();
        }

        // Send stop command to core
        CoreInteropRequest req("audio_stream_stop");
        req.AddParam("SessionHandle", handle);
        std::string json = req.ToJson();

        auto response = core_->call(req.Command(), *logger_, &json);

        // Enqueue the final transcription result from the stop response, then close
        if (resultQueue_) {
            if (response.HasError()) {
                resultQueue_->CloseWithError("audio_stream_stop failed: " + response.error);
            }
            else {
                if (!response.data.empty()) {
                    try {
                        auto finalResult = LiveAudioTranscriptionResponse::FromJson(response.data);
                        resultQueue_->Push(std::move(finalResult));
                    }
                    catch (const std::exception& e) {
                        logger_->Log(LogLevel::Warning,
                                     std::string("Failed to parse final transcription response: ") + e.what());
                    }
                }
                resultQueue_->Close();
            }
        }

        lock.lock();
    }

    void LiveAudioTranscriptionSession::PushWorkerLoop() {
        AudioChunk chunk;
        while (true) {
            auto status = pushQueue_->Pop(chunk);
            if (status != Internal::DequeueStatus::Item) {
                break;
            }

            std::string handle;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                handle = sessionHandle_;
            }

            CoreInteropRequest req("audio_stream_push");
            req.AddParam("SessionHandle", handle);
            std::string json = req.ToJson();

            auto response = core_->callWithBinary(req.Command(), *logger_, &json,
                                                   chunk.data(), chunk.size());

            if (response.HasError()) {
                auto coreError = CoreErrorResponse::TryParse(response.error);
                std::string msg = coreError.has_value() ? coreError->message : response.error;

                logger_->Log(LogLevel::Error, "audio_stream_push failed: " + msg);
                pushQueue_->Close();
                resultQueue_->CloseWithError(msg);

                std::lock_guard<std::mutex> lock(mutex_);
                errorMessage_ = std::move(msg);
                return;
            }

            // Parse the response as a transcription result if there is data
            if (!response.data.empty()) {
                try {
                    auto result = LiveAudioTranscriptionResponse::FromJson(response.data);
                    resultQueue_->Push(std::move(result));
                }
                catch (const std::exception& e) {
                    logger_->Log(LogLevel::Warning,
                                 std::string("Failed to parse transcription response: ") + e.what());
                }
            }
        }
    }

    std::string LiveAudioTranscriptionSession::GetErrorMessage() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return errorMessage_;
    }

    bool LiveAudioTranscriptionSession::IsStarted() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_ == SessionState::Started;
    }

    bool LiveAudioTranscriptionSession::IsStopped() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_ == SessionState::Stopped;
    }

} // namespace foundry_local
