// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <sstream>
#include <utility>

namespace fl {

template <typename... Args>
std::string MakeString(Args&&... args) {
  std::ostringstream out;
  (out << ... << std::forward<Args>(args));
  return out.str();
}

}  // namespace fl
