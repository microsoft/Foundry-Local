// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "inferencing/generative/embeddings/fp16.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

using fl::Fp16ToFp32;

namespace {

/// Reinterpret a float as its raw uint32 bit pattern.
uint32_t BitsOf(float v) {
  uint32_t bits = 0;
  std::memcpy(&bits, &v, sizeof(bits));
  return bits;
}

}  // namespace

// ===========================================================================
// Zeros
// ===========================================================================

TEST(Fp16ToFp32Test, Zeros) {
  const float pos_zero = Fp16ToFp32(0x0000);
  EXPECT_EQ(BitsOf(pos_zero), 0x00000000u);
  EXPECT_EQ(pos_zero, 0.0f);

  const float neg_zero = Fp16ToFp32(0x8000);
  EXPECT_EQ(BitsOf(neg_zero), 0x80000000u);
  EXPECT_EQ(neg_zero, 0.0f);  // -0.0 == 0.0 numerically
  EXPECT_TRUE(std::signbit(neg_zero));
}

// ===========================================================================
// Normal values
// ===========================================================================

TEST(Fp16ToFp32Test, NormalValues) {
  EXPECT_FLOAT_EQ(Fp16ToFp32(0x3C00), 1.0f);
  EXPECT_FLOAT_EQ(Fp16ToFp32(0xBC00), -1.0f);
  EXPECT_FLOAT_EQ(Fp16ToFp32(0x4000), 2.0f);
  EXPECT_FLOAT_EQ(Fp16ToFp32(0xC000), -2.0f);

  // 0x3555 ≈ 1/3. Sign=0, exp=0x0D (13), mantissa=0x155.
  // value = (1 + 0x155/1024) * 2^(13-15) = 1.33301... * 0.25 = 0.333251953125
  EXPECT_FLOAT_EQ(Fp16ToFp32(0x3555), 0.333251953125f);

  // Max finite: 0x7BFF -> 65504.0
  EXPECT_FLOAT_EQ(Fp16ToFp32(0x7BFF), 65504.0f);
  EXPECT_FLOAT_EQ(Fp16ToFp32(0xFBFF), -65504.0f);
}

// ===========================================================================
// Subnormals
// ===========================================================================

TEST(Fp16ToFp32Test, Subnormals) {
  // Smallest positive subnormal: 0x0001 = 2^-24
  const float smallest_sub = Fp16ToFp32(0x0001);
  EXPECT_FLOAT_EQ(smallest_sub, std::ldexp(1.0f, -24));

  // Largest subnormal: 0x03FF = (1023/1024) * 2^-14
  const float largest_sub = Fp16ToFp32(0x03FF);
  const float expected_largest_sub = (1023.0f / 1024.0f) * std::ldexp(1.0f, -14);
  EXPECT_NEAR(largest_sub, expected_largest_sub, 1e-10f);

  // Smallest positive normal: 0x0400 = 2^-14
  const float smallest_normal = Fp16ToFp32(0x0400);
  EXPECT_FLOAT_EQ(smallest_normal, std::ldexp(1.0f, -14));

  // Continuity: smallest normal should be just above largest subnormal,
  // and the gap should equal one fp16 ULP at the subnormal level (2^-24).
  EXPECT_GT(smallest_normal, largest_sub);
  EXPECT_NEAR(smallest_normal - largest_sub, std::ldexp(1.0f, -24), 1e-10f);
}

// ===========================================================================
// Infinities
// ===========================================================================

TEST(Fp16ToFp32Test, Infinities) {
  const float pos_inf = Fp16ToFp32(0x7C00);
  EXPECT_TRUE(std::isinf(pos_inf));
  EXPECT_GT(pos_inf, 0.0f);

  const float neg_inf = Fp16ToFp32(0xFC00);
  EXPECT_TRUE(std::isinf(neg_inf));
  EXPECT_LT(neg_inf, 0.0f);
}

// ===========================================================================
// NaN
// ===========================================================================

TEST(Fp16ToFp32Test, NaN) {
  const float nan_pos = Fp16ToFp32(0x7E00);
  EXPECT_TRUE(std::isnan(nan_pos));

  const float nan_neg = Fp16ToFp32(0xFE00);
  EXPECT_TRUE(std::isnan(nan_neg));
  // The sign bit should be preserved through the conversion.
  EXPECT_TRUE(std::signbit(nan_neg));
}

// ===========================================================================
// Subnormal exact-bit checks. Pin the exact fp32 bit pattern so future
// refactors of the subnormal-normalization loop (which uses uint32_t arithmetic
// for the exponent) can't silently drift due to integer wraparound.
// ===========================================================================

TEST(Fp16ToFp32Test, SubnormalExactBits) {
  // h=0x0001 → 2^-24. fp32 sign=0, exponent=103 (0x67), mantissa=0.
  EXPECT_EQ(BitsOf(Fp16ToFp32(0x0001)), 0x33800000u);

  // h=0x8001 → -2^-24. Sign bit set, otherwise identical exponent/mantissa.
  EXPECT_EQ(BitsOf(Fp16ToFp32(0x8001)), 0xB3800000u);

  // h=0x0200 → 2^-15. fp32 sign=0, exponent=112 (0x70), mantissa=0.
  EXPECT_EQ(BitsOf(Fp16ToFp32(0x0200)), 0x38000000u);

  // h=0x03FF → 1023 * 2^-24. fp32 sign=0, exponent=112, mantissa=0x7FC000.
  EXPECT_EQ(BitsOf(Fp16ToFp32(0x03FF)), 0x387FC000u);
}
