// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <gtest/gtest.h>

#include "mock_core.h"
#include "mock_object_factory.h"
#include "foundry_local_exception.h"

#include "openai/openai_live_audio_types.h"
#include "openai/openai_live_audio_client.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <string>
#include <thread>
#include <vector>

using namespace foundry_local;
using namespace foundry_local::Testing;

// ---------------------------------------------------------------------------
// LiveAudioTranscriptionResponse parsing tests
// ---------------------------------------------------------------------------

TEST(LiveAudioTypesTest, FromJson_BasicResponse) {
    nlohmann::json j = {
        {"text", "hello world"},
        {"is_final", true},
        {"start_time", 0.5},
        {"end_time", 1.5}};

    auto resp = LiveAudioTranscriptionResponse::FromJson(j.dump());
    EXPECT_EQ("hello world", resp.text);
    EXPECT_TRUE(resp.is_final);
    ASSERT_TRUE(resp.start_time.has_value());
    EXPECT_DOUBLE_EQ(0.5, resp.start_time.value());
    ASSERT_TRUE(resp.end_time.has_value());
    EXPECT_DOUBLE_EQ(1.5, resp.end_time.value());
}

TEST(LiveAudioTypesTest, FromJson_CamelCaseFields) {
    nlohmann::json j = {
        {"text", "test"},
        {"isFinal", false},
        {"startTime", 1.0},
        {"endTime", 2.0}};

    auto resp = LiveAudioTranscriptionResponse::FromJson(j.dump());
    EXPECT_EQ("test", resp.text);
    EXPECT_FALSE(resp.is_final);
    ASSERT_TRUE(resp.start_time.has_value());
    EXPECT_DOUBLE_EQ(1.0, resp.start_time.value());
}

TEST(LiveAudioTypesTest, FromJson_WithContent) {
    nlohmann::json j = {
        {"text", "hello"},
        {"is_final", true},
        {"content", {{{"text", "hi"}, {"transcript", "hi there"}}}}};

    auto resp = LiveAudioTranscriptionResponse::FromJson(j.dump());
    ASSERT_EQ(1u, resp.content.size());
    EXPECT_EQ("hi", resp.content[0].text);
    EXPECT_EQ("hi there", resp.content[0].transcript);
}

TEST(LiveAudioTypesTest, FromJson_EmptyJson) {
    auto resp = LiveAudioTranscriptionResponse::FromJson("{}");
    EXPECT_TRUE(resp.text.empty());
    EXPECT_FALSE(resp.is_final);
    EXPECT_FALSE(resp.start_time.has_value());
    EXPECT_FALSE(resp.end_time.has_value());
    EXPECT_TRUE(resp.content.empty());
}

TEST(LiveAudioTypesTest, CoreErrorResponse_TryParse_Valid) {
    nlohmann::json j = {
        {"code", "RATE_LIMITED"},
        {"message", "Too many requests"},
        {"is_transient", true}};

    auto result = CoreErrorResponse::TryParse(j.dump());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ("RATE_LIMITED", result->code);
    EXPECT_EQ("Too many requests", result->message);
    EXPECT_TRUE(result->is_transient);
}

TEST(LiveAudioTypesTest, CoreErrorResponse_TryParse_Invalid) {
    auto result = CoreErrorResponse::TryParse("not json");
    EXPECT_FALSE(result.has_value());
}

// ---------------------------------------------------------------------------
// LiveAudioTranscriptionSession tests
// ---------------------------------------------------------------------------

class LiveAudioSessionTest : public ::testing::Test {
protected:
    MockCore core_;
    NullLogger logger_;

    void SetUpStartHandlers(const std::string& sessionHandle = "session-123") {
        core_.OnCall("audio_stream_start", sessionHandle);
    }

    void SetUpPushHandler(const std::string& responseJson = "") {
        core_.OnCall("audio_stream_push",
                     [responseJson](std::string_view, const std::string*, NativeCallbackFn, void*) {
                         return responseJson;
                     });
    }

    void SetUpStopHandler() {
        core_.OnCall("audio_stream_stop", "");
    }

    void SetUpAllHandlers(const std::string& pushResponse = "") {
        SetUpStartHandlers();
        SetUpPushHandler(pushResponse);
        SetUpStopHandler();
    }
};

TEST_F(LiveAudioSessionTest, ConstructorDefaults) {
    LiveAudioTranscriptionSession session(&core_, "whisper-model", &logger_);
    EXPECT_FALSE(session.IsStarted());
    EXPECT_FALSE(session.IsStopped());
    EXPECT_EQ(16000, session.Settings().sample_rate);
    EXPECT_EQ(1, session.Settings().channels);
    EXPECT_EQ(16, session.Settings().bits_per_sample);
}

TEST_F(LiveAudioSessionTest, SettingsCanBeModifiedBeforeStart) {
    LiveAudioTranscriptionSession session(&core_, "whisper-model", &logger_);
    session.Settings().sample_rate = 44100;
    session.Settings().channels = 2;
    session.Settings().language = "en";

    EXPECT_EQ(44100, session.Settings().sample_rate);
    EXPECT_EQ(2, session.Settings().channels);
    EXPECT_EQ("en", session.Settings().language.value());
}

TEST_F(LiveAudioSessionTest, Start_Success) {
    SetUpAllHandlers();

    LiveAudioTranscriptionSession session(&core_, "whisper-model", &logger_);
    session.Start();

    EXPECT_TRUE(session.IsStarted());
    EXPECT_FALSE(session.IsStopped());
    EXPECT_EQ(16000, session.ActiveSettings().sample_rate);

    session.Stop();
    EXPECT_TRUE(session.IsStopped());
}

TEST_F(LiveAudioSessionTest, Start_WithCustomSettings) {
    SetUpAllHandlers();

    LiveAudioTranscriptionSession session(&core_, "whisper-model", &logger_);
    session.Settings().sample_rate = 44100;
    session.Settings().language = "fr";
    session.Start();

    EXPECT_EQ(44100, session.ActiveSettings().sample_rate);
    EXPECT_EQ("fr", session.ActiveSettings().language.value());

    // Verify the request included our settings
    auto lastArg = core_.GetLastDataArg("audio_stream_start");
    auto parsed = nlohmann::json::parse(lastArg);
    EXPECT_EQ("44100", parsed["Params"]["SampleRate"].get<std::string>());
    EXPECT_EQ("fr", parsed["Params"]["Language"].get<std::string>());

    session.Stop();
}

TEST_F(LiveAudioSessionTest, Start_Failure) {
    core_.OnCallThrow("audio_stream_start", "Connection refused");

    LiveAudioTranscriptionSession session(&core_, "whisper-model", &logger_);
    EXPECT_THROW(session.Start(), Exception);
    EXPECT_FALSE(session.IsStarted());
}

TEST_F(LiveAudioSessionTest, Start_EmptyHandle) {
    core_.OnCall("audio_stream_start", "");

    LiveAudioTranscriptionSession session(&core_, "whisper-model", &logger_);
    EXPECT_THROW(session.Start(), Exception);
    EXPECT_FALSE(session.IsStarted());
}

TEST_F(LiveAudioSessionTest, DoubleStartThrows) {
    SetUpAllHandlers();

    LiveAudioTranscriptionSession session(&core_, "whisper-model", &logger_);
    session.Start();
    EXPECT_THROW(session.Start(), Exception);

    session.Stop();
}

TEST_F(LiveAudioSessionTest, AppendBeforeStartThrows) {
    LiveAudioTranscriptionSession session(&core_, "whisper-model", &logger_);
    std::vector<uint8_t> data = {0, 1, 2, 3};
    EXPECT_THROW(session.Append(data.data(), data.size()), Exception);
}

TEST_F(LiveAudioSessionTest, StopParseFinalResponse) {
    SetUpStartHandlers();
    SetUpPushHandler();

    // audio_stream_stop returns a final transcription result
    nlohmann::json finalResponse = {
        {"text", "final result"},
        {"is_final", true}};
    core_.OnCall("audio_stream_stop", finalResponse.dump());

    LiveAudioTranscriptionSession session(&core_, "whisper-model", &logger_);
    session.Start();
    session.Stop();

    // The final result should be retrievable from the result queue
    LiveAudioTranscriptionResponse result;
    auto status = session.TryGetNext(result, std::chrono::milliseconds(100));
    EXPECT_EQ(TranscriptionStatus::Result, status);
    EXPECT_EQ("final result", result.text);
    EXPECT_TRUE(result.is_final);
}

TEST_F(LiveAudioSessionTest, AppendAndGetResult) {
    nlohmann::json pushResponse = {
        {"text", "hello"},
        {"is_final", false}};
    SetUpAllHandlers(pushResponse.dump());

    LiveAudioTranscriptionSession session(&core_, "whisper-model", &logger_);
    session.Start();

    // Append some data
    std::vector<uint8_t> data(320, 0);
    session.Append(data.data(), data.size());

    // Try to get a result
    LiveAudioTranscriptionResponse result;
    auto status = session.TryGetNext(result, std::chrono::seconds(2));

    if (status == TranscriptionStatus::Result) {
        EXPECT_EQ("hello", result.text);
        EXPECT_FALSE(result.is_final);
    }

    session.Stop();
}

TEST_F(LiveAudioSessionTest, StopSendsCommand) {
    SetUpAllHandlers();

    LiveAudioTranscriptionSession session(&core_, "whisper-model", &logger_);
    session.Start();
    session.Stop();

    EXPECT_EQ(1, core_.GetCallCount("audio_stream_stop"));

    auto lastArg = core_.GetLastDataArg("audio_stream_stop");
    auto parsed = nlohmann::json::parse(lastArg);
    EXPECT_EQ("session-123", parsed["Params"]["SessionHandle"].get<std::string>());
}

TEST_F(LiveAudioSessionTest, StopWhenNotStartedIsNoop) {
    LiveAudioTranscriptionSession session(&core_, "whisper-model", &logger_);
    session.Stop(); // Should not throw
    EXPECT_EQ(0, core_.GetCallCount("audio_stream_stop"));
}

TEST_F(LiveAudioSessionTest, DoubleStopIsNoop) {
    SetUpAllHandlers();

    LiveAudioTranscriptionSession session(&core_, "whisper-model", &logger_);
    session.Start();
    session.Stop();
    session.Stop(); // Should not throw or send a second command
    EXPECT_EQ(1, core_.GetCallCount("audio_stream_stop"));
}

TEST_F(LiveAudioSessionTest, DestructorStopsSession) {
    SetUpAllHandlers();

    {
        LiveAudioTranscriptionSession session(&core_, "whisper-model", &logger_);
        session.Start();
        // Destructor should call Stop
    }

    EXPECT_EQ(1, core_.GetCallCount("audio_stream_stop"));
}

TEST_F(LiveAudioSessionTest, TryGetNextTimeout) {
    SetUpAllHandlers();

    LiveAudioTranscriptionSession session(&core_, "whisper-model", &logger_);
    session.Start();

    LiveAudioTranscriptionResponse result;
    auto status = session.TryGetNext(result, std::chrono::milliseconds(50));
    EXPECT_EQ(TranscriptionStatus::Timeout, status);

    session.Stop();
}

TEST_F(LiveAudioSessionTest, GetErrorMessage_NoError) {
    LiveAudioTranscriptionSession session(&core_, "whisper-model", &logger_);
    EXPECT_TRUE(session.GetErrorMessage().empty());
}
