// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace fl {

/// Convert 16-bit signed little-endian PCM bytes to float32 samples in [-1.0, 1.0].
/// Each pair of bytes becomes one float sample. A trailing odd byte is ignored.
std::vector<float> ConvertS16LEToFloat(const uint8_t* pcm_bytes, size_t byte_count);

}  // namespace fl
