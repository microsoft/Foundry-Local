// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "inferencing/generative/audio/pcm_utils.h"
#include "exception.h"

#include <array>
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

std::vector<uint8_t> AudioInternal::EncodePcm16Wav(std::span<const uint8_t> pcm) {
  if (pcm.empty() || pcm.size() % sizeof(int16_t) != 0) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_USAGE, "Buffered PCM audio must contain complete 16-bit samples.");
  }
  if (pcm.size() > kMaxBufferedPcmBytes) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_USAGE, "Buffered PCM audio exceeds the 64 MiB utterance limit.");
  }

  std::array<uint8_t, 44> header{
      'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E',
      'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 1, 0,
      0x80, 0x3e, 0, 0, 0x00, 0x7d, 0, 0, 2, 0, 16, 0,
      'd', 'a', 't', 'a', 0, 0, 0, 0};
  const auto data_size = static_cast<uint32_t>(pcm.size());
  for (size_t i = 0; i < sizeof(uint32_t); ++i) {
    header[4 + i] = static_cast<uint8_t>((36 + data_size) >> (8 * i));
    header[40 + i] = static_cast<uint8_t>(data_size >> (8 * i));
  }

  std::vector<uint8_t> wav;
  wav.reserve(header.size() + pcm.size());
  wav.insert(wav.end(), header.begin(), header.end());
  wav.insert(wav.end(), pcm.begin(), pcm.end());
  return wav;
}

}  // namespace fl
