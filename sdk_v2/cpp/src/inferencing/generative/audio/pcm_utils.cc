// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "inferencing/generative/audio/pcm_utils.h"

#include <cstring>

namespace fl {

std::vector<float> ConvertS16LEToFloat(const uint8_t* pcm_bytes, size_t byte_count) {
  const size_t sample_count = byte_count / 2;
  std::vector<float> samples(sample_count);

  for (size_t i = 0; i < sample_count; ++i) {
    // Little-endian: low byte first, high byte second.
    int16_t sample;
    std::memcpy(&sample, pcm_bytes + i * 2, sizeof(int16_t));
    samples[i] = static_cast<float>(sample) / 32768.0f;
  }

  return samples;
}

}  // namespace fl
