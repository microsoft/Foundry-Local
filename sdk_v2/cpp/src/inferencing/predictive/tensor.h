// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace fl {

// -----------------------------------------------------------------------
// Tensor types
// -----------------------------------------------------------------------

/// Tensor dimension descriptor.
struct TensorShape {
  std::vector<int64_t> dims;

  int64_t element_count() const;
  int64_t rank() const;
};

enum class ElementType {
  kFloat32,
  kFloat16,
  kBFloat16,
  kInt8,
  kUInt8,
  kInt16,
  kUInt16,
  kInt32,
  kUInt32,
  kInt64,
  kUInt64,
  kBool,
  kString,
  kFloat64,
};

class Tensor {
 public:
  Tensor(ElementType type, TensorShape shape, const void* data, size_t data_size);
  Tensor(ElementType type, TensorShape shape);
  ~Tensor();
  Tensor(Tensor&&) noexcept;
  Tensor& operator=(Tensor&&) noexcept;
  Tensor(const Tensor&) = delete;
  Tensor& operator=(const Tensor&) = delete;

  ElementType element_type() const { return element_type_; }
  const TensorShape& shape() const { return shape_; }
  size_t data_size() const { return buffer_.size(); }
  const void* data() const { return buffer_.data(); }
  void* mutable_data() { return buffer_.data(); }
  int64_t element_count() const { return shape_.element_count(); }

  static size_t element_size(ElementType type);

 private:
  ElementType element_type_;
  TensorShape shape_;
  std::vector<uint8_t> buffer_;
};

}  // namespace fl
