// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
// Tests for AudioTranscriptionResponse JSON serialization.

#include "contracts/audio_transcriptions.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using namespace fl;

TEST(AudioTranscriptionResponseJsonTest, ToJson_EmitsIdWhenNonEmpty) {
  AudioTranscriptionResponse r{.id = "audio_xyz", .text = "hi"};
  nlohmann::json j = r;

  ASSERT_TRUE(j.contains("id"));
  EXPECT_EQ(j.at("id").get<std::string>(), "audio_xyz");
  EXPECT_EQ(j.at("text").get<std::string>(), "hi");

  // Also check the serialized string contains the field.
  EXPECT_NE(j.dump().find("\"id\":\"audio_xyz\""), std::string::npos);
}

TEST(AudioTranscriptionResponseJsonTest, ToJson_OmitsIdWhenEmpty) {
  AudioTranscriptionResponse r{.id = "", .text = "hi"};
  nlohmann::json j = r;

  EXPECT_FALSE(j.contains("id"));
  EXPECT_EQ(j.at("text").get<std::string>(), "hi");
  EXPECT_EQ(j.dump().find("\"id\""), std::string::npos);
}
