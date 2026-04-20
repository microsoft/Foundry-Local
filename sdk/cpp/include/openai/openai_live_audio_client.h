// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <vector>

#include <gsl/pointers>

#include "openai_live_audio_types.h"

namespace foundry_local::Internal {
    struct IFoundryLocalCore;
    template <typename T> class ThreadSafeQueue;
} // namespace foundry_local::Internal

namespace foundry_local {
    class ILogger;

    class LiveAudioTranscriptionSession final {
    public:
        LiveAudioTranscriptionSession(gsl::not_null<Internal::IFoundryLocalCore*> core,
                                       std::string modelId,
                                       gsl::not_null<ILogger*> logger);
        ~LiveAudioTranscriptionSession() noexcept;

        // Non-copyable, non-movable
        LiveAudioTranscriptionSession(const LiveAudioTranscriptionSession&) = delete;
        LiveAudioTranscriptionSession& operator=(const LiveAudioTranscriptionSession&) = delete;
        LiveAudioTranscriptionSession(LiveAudioTranscriptionSession&&) = delete;
        LiveAudioTranscriptionSession& operator=(LiveAudioTranscriptionSession&&) = delete;

        /// Mutable settings reference; only effective before Start().
        LiveAudioTranscriptionOptions& Settings() { return settings_; }
        /// Read-only settings reference.
        const LiveAudioTranscriptionOptions& Settings() const { return settings_; }
        /// Settings that were active when Start() was called.
        const LiveAudioTranscriptionOptions& ActiveSettings() const { return activeSettings_; }

        /// Begin the streaming session. Must be called before Append/TryAppend.
        void Start();

        /// Enqueue PCM audio data. Blocks if the push queue is full.
        void Append(const uint8_t* pcmData, size_t length);

        /// Try to enqueue PCM audio data without blocking. Returns false if the queue is full.
        bool TryAppend(const uint8_t* pcmData, size_t length);

        /// Try to enqueue PCM audio data with a timeout. Returns false on timeout.
        bool TryAppendFor(const uint8_t* pcmData, size_t length, std::chrono::milliseconds timeout);

        /// Try to get the next transcription result within the given timeout.
        TranscriptionStatus TryGetNext(LiveAudioTranscriptionResponse& result,
                                        std::chrono::milliseconds timeout = std::chrono::seconds(5));

        /// Signal the end of audio input and stop the session.
        void Stop();

        /// Returns the error message if the session is in an error state.
        std::string GetErrorMessage() const;

        /// Returns true if the session has been started.
        bool IsStarted() const;

        /// Returns true if the session has been stopped.
        bool IsStopped() const;

    private:
        enum class SessionState {
            Created,
            Starting,
            Started,
            Stopped
        };

        void PushWorkerLoop();
        void StopInternal(std::unique_lock<std::mutex>& lock);

        gsl::not_null<Internal::IFoundryLocalCore*> core_;
        std::string modelId_;
        gsl::not_null<ILogger*> logger_;

        LiveAudioTranscriptionOptions settings_;
        LiveAudioTranscriptionOptions activeSettings_;

        mutable std::mutex mutex_;
        SessionState state_ = SessionState::Created;
        std::string sessionHandle_;

        using AudioChunk = std::vector<uint8_t>;
        std::unique_ptr<Internal::ThreadSafeQueue<AudioChunk>> pushQueue_;
        std::unique_ptr<Internal::ThreadSafeQueue<LiveAudioTranscriptionResponse>> resultQueue_;

        std::thread pushThread_;
        std::string errorMessage_;
    };

} // namespace foundry_local
