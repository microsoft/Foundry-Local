// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <cstdint>
#include <cstring>

namespace fl {

/// IEEE 754 half-precision (uint16_t) to single-precision (float) conversion.
/// Handles subnormals, +/-Inf, and NaN. The sign of NaN is preserved; the mantissa is
/// shifted into the upper bits of the FP32 mantissa so the value is still a NaN
/// (not a denormal or zero).
inline float Fp16ToFp32(uint16_t h) {
  uint32_t sign = (h & 0x8000u) << 16;
  uint32_t exponent = (h >> 10) & 0x1F;
  uint32_t mantissa = h & 0x03FF;

  uint32_t f;
  if (exponent == 0) {
    if (mantissa == 0) {
      f = sign;  // +/-zero
    } else {
      // Subnormal: normalize.
      exponent = 1;
      while ((mantissa & 0x0400) == 0) {
        mantissa <<= 1;
        exponent--;
      }
      mantissa &= 0x03FF;
      f = sign | ((exponent + 127 - 15) << 23) | (mantissa << 13);
    }
  } else if (exponent == 31) {
    f = sign | 0x7F800000u | (mantissa << 13);  // Inf/NaN
  } else {
    f = sign | ((exponent + 127 - 15) << 23) | (mantissa << 13);
  }

  float val;
  std::memcpy(&val, &f, sizeof(float));
  return val;
}

}  // namespace fl
