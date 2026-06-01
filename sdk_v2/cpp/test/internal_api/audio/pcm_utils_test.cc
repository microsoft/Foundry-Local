// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Unit tests for PCM s16le → float32 conversion.
//

#include "inferencing/generative/audio/pcm_utils.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <vector>

using namespace fl;

TEST(PcmUtilsTest, Silence_AllZeros) {
  std::vector<uint8_t> pcm(64, 0);
  auto result = ConvertS16LEToFloat(pcm.data(), pcm.size());

  EXPECT_EQ(result.size(), 32u);
  for (float sample : result) {
    EXPECT_FLOAT_EQ(sample, 0.0f);
  }
}

TEST(PcmUtilsTest, KnownValues) {
  // 0x0100 in LE = 256 as int16_t → 256/32768 ≈ 0.0078125
  uint8_t pcm[] = {0x00, 0x01};
  auto result = ConvertS16LEToFloat(pcm, sizeof(pcm));

  ASSERT_EQ(result.size(), 1u);
  EXPECT_FLOAT_EQ(result[0], 256.0f / 32768.0f);
}

TEST(PcmUtilsTest, MaxPositive) {
  // INT16_MAX = 32767 = 0xFF7F in LE
  uint8_t pcm[] = {0xFF, 0x7F};
  auto result = ConvertS16LEToFloat(pcm, sizeof(pcm));

  ASSERT_EQ(result.size(), 1u);
  EXPECT_NEAR(result[0], 1.0f, 0.001f);
}

TEST(PcmUtilsTest, MaxNegative) {
  // INT16_MIN = -32768 = 0x0080 in LE
  uint8_t pcm[] = {0x00, 0x80};
  auto result = ConvertS16LEToFloat(pcm, sizeof(pcm));

  ASSERT_EQ(result.size(), 1u);
  EXPECT_FLOAT_EQ(result[0], -1.0f);
}

TEST(PcmUtilsTest, OddByteCount_TrailingByteIgnored) {
  // 3 bytes → only 1 sample (first 2 bytes), trailing byte ignored
  uint8_t pcm[] = {0x00, 0x01, 0xFF};
  auto result = ConvertS16LEToFloat(pcm, sizeof(pcm));

  ASSERT_EQ(result.size(), 1u);
  EXPECT_FLOAT_EQ(result[0], 256.0f / 32768.0f);
}

TEST(PcmUtilsTest, EmptyInput) {
  auto result = ConvertS16LEToFloat(nullptr, 0);
  EXPECT_TRUE(result.empty());
}

TEST(PcmUtilsTest, MultipleSamples) {
  // Two samples: 0 and -1 (0xFFFF = -1 in int16_t)
  uint8_t pcm[] = {0x00, 0x00, 0xFF, 0xFF};
  auto result = ConvertS16LEToFloat(pcm, sizeof(pcm));

  ASSERT_EQ(result.size(), 2u);
  EXPECT_FLOAT_EQ(result[0], 0.0f);
  EXPECT_FLOAT_EQ(result[1], -1.0f / 32768.0f);
}
