// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "inferencing/predictive/inference_session.h"
#include "exception.h"

#include <cstring>

namespace fl {

// ---------------------------------------------------------------------------
// Tensor
// ---------------------------------------------------------------------------

Tensor::Tensor(ElementType type, TensorShape shape, const void* data, size_t data_size)
    : element_type_(type), shape_(std::move(shape)), buffer_(data_size) {
  if (data && data_size > 0) {
    std::memcpy(buffer_.data(), data, data_size);
  }
}

Tensor::Tensor(ElementType type, TensorShape shape)
    : element_type_(type), shape_(std::move(shape)) {
  size_t count = static_cast<size_t>(shape_.element_count());
  buffer_.resize(count * element_size(type), 0);
}

Tensor::Tensor(Tensor&&) noexcept = default;
Tensor& Tensor::operator=(Tensor&&) noexcept = default;
Tensor::~Tensor() = default;

size_t Tensor::element_size(ElementType type) {
  switch (type) {
    case ElementType::kFloat32:
      return 4;
    case ElementType::kFloat16:
      return 2;
    case ElementType::kBFloat16:
      return 2;
    case ElementType::kInt8:
      return 1;
    case ElementType::kUInt8:
      return 1;
    case ElementType::kInt16:
      return 2;
    case ElementType::kUInt16:
      return 2;
    case ElementType::kInt32:
      return 4;
    case ElementType::kUInt32:
      return 4;
    case ElementType::kInt64:
      return 8;
    case ElementType::kUInt64:
      return 8;
    case ElementType::kBool:
      return 1;
    case ElementType::kString:
      return 0;  // Variable-length
    case ElementType::kFloat64:
      return 8;
  }
  return 0;
}

// ---------------------------------------------------------------------------
// InferenceResult
// ---------------------------------------------------------------------------

const Tensor* InferenceResult::Get(const std::string& name) const {
  for (size_t i = 0; i < output_names.size(); ++i) {
    if (output_names[i] == name && i < output_tensors.size()) {
      return &output_tensors[i];
    }
  }
  return nullptr;
}

// ---------------------------------------------------------------------------
// InferenceSession
// ---------------------------------------------------------------------------

InferenceSession::InferenceSession() = default;
InferenceSession::~InferenceSession() = default;

std::unique_ptr<InferenceSession> InferenceSession::Create(
    const std::string& /*model_path*/, const InferenceSessionOptions& /*options*/) {
  FL_THROW(FOUNDRY_LOCAL_ERROR_NOT_IMPLEMENTED, "InferenceSession::Create");
}

std::unique_ptr<InferenceSession> InferenceSession::Create(
    const void* /*model_data*/, size_t /*model_size*/,
    const InferenceSessionOptions& /*options*/) {
  FL_THROW(FOUNDRY_LOCAL_ERROR_NOT_IMPLEMENTED, "InferenceSession::Create");
}

std::vector<InferenceSession::InputInfo> InferenceSession::GetInputInfo() const {
  FL_THROW(FOUNDRY_LOCAL_ERROR_NOT_IMPLEMENTED, "InferenceSession::GetInputInfo");
}

std::vector<InferenceSession::OutputInfo> InferenceSession::GetOutputInfo() const {
  FL_THROW(FOUNDRY_LOCAL_ERROR_NOT_IMPLEMENTED, "InferenceSession::GetOutputInfo");
}

InferenceResult InferenceSession::Run(const std::vector<std::string>& /*input_names*/,
                                      const std::vector<Tensor>& /*input_tensors*/,
                                      const std::vector<std::string>& /*output_names*/) {
  FL_THROW(FOUNDRY_LOCAL_ERROR_NOT_IMPLEMENTED, "InferenceSession::Run");
}

}  // namespace fl
