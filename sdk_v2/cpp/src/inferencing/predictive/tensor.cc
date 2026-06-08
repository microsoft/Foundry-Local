// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "inferencing/predictive/tensor.h"

#include <numeric>

namespace fl {

int64_t TensorShape::element_count() const {
  if (dims.empty()) {
    return 0;
  }

  return std::accumulate(dims.begin(), dims.end(), int64_t{1}, std::multiplies<int64_t>());
}

int64_t TensorShape::rank() const {
  return static_cast<int64_t>(dims.size());
}

}  // namespace fl
