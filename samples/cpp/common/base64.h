// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Minimal standard Base64 encoder used to embed image bytes in a Responses API
// `data:` URL. Header-only and dependency-free.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sample {

/// Encode raw bytes as standard (RFC 4648) Base64 with '=' padding.
inline std::string Base64Encode(const uint8_t* data, size_t size) {
  static constexpr char kChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  std::string out;
  out.reserve(((size + 2) / 3) * 4);

  size_t i = 0;
  while (i < size) {
    const uint32_t octet_a = i < size ? data[i++] : 0;
    const uint32_t octet_b = i < size ? data[i++] : 0;
    const uint32_t octet_c = i < size ? data[i++] : 0;
    const uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

    out.push_back(kChars[(triple >> 18) & 0x3F]);
    out.push_back(kChars[(triple >> 12) & 0x3F]);
    out.push_back(kChars[(triple >> 6) & 0x3F]);
    out.push_back(kChars[triple & 0x3F]);
  }

  // Apply '=' padding for the trailing partial group.
  if (const size_t mod = size % 3; mod == 1) {
    out[out.size() - 2] = '=';
    out[out.size() - 1] = '=';
  } else if (mod == 2) {
    out[out.size() - 1] = '=';
  }

  return out;
}

inline std::string Base64Encode(const std::vector<uint8_t>& data) {
  return Base64Encode(data.data(), data.size());
}

}  // namespace sample
