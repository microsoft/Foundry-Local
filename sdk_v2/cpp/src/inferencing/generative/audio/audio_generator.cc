// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "inferencing/generative/audio/audio_generator.h"

namespace fl {

std::string AudioGenerator::GenerateAll() {
  std::string result;

  while (!IsDone()) {
    GenerateNextToken();
    result += Decode();
  }

  return result;
}

}  // namespace fl
