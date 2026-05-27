// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "inferencing/predictive/tensor.h"

#include <memory>
#include <string>
#include <vector>

namespace fl {

// -----------------------------------------------------------------------
// Inference session types
// -----------------------------------------------------------------------

struct InferenceSessionOptions {
  // Will hold ORT session options in Phase 4.
};

struct InferenceResult {
  std::vector<std::string> output_names;
  std::vector<Tensor> output_tensors;
  std::string result_json;

  const Tensor* Get(const std::string& name) const;
};

class InferenceSession {
 public:
  struct InputInfo {
    std::string name;
    ElementType element_type;
    TensorShape shape;
  };
  struct OutputInfo {
    std::string name;
    ElementType element_type;
    TensorShape shape;
  };

  InferenceSession();
  ~InferenceSession();
  InferenceSession(const InferenceSession&) = delete;
  InferenceSession& operator=(const InferenceSession&) = delete;

  static std::unique_ptr<InferenceSession> Create(
      const std::string& model_path, const InferenceSessionOptions& options);
  static std::unique_ptr<InferenceSession> Create(
      const void* model_data, size_t model_size,
      const InferenceSessionOptions& options);

  std::vector<InputInfo> GetInputInfo() const;
  std::vector<OutputInfo> GetOutputInfo() const;

  InferenceResult Run(const std::vector<std::string>& input_names,
                      const std::vector<Tensor>& input_tensors,
                      const std::vector<std::string>& output_names);
};

}  // namespace fl
