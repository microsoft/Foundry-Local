// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Tests for Tensor (constructor, element_size), TensorShape, and InferenceResult::Get.
//
#include "inferencing/predictive/tensor.h"
#include "inferencing/predictive/inference_session.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

using namespace fl;

// ========================================================================
// TensorShape
// ========================================================================

TEST(TensorShapeTest, EmptyDimsReturnsZeroCount) {
  TensorShape shape;
  EXPECT_EQ(shape.element_count(), 0);
  EXPECT_EQ(shape.rank(), 0);
}

TEST(TensorShapeTest, SingleDim) {
  TensorShape shape;
  shape.dims = {10};
  EXPECT_EQ(shape.element_count(), 10);
  EXPECT_EQ(shape.rank(), 1);
}

TEST(TensorShapeTest, MultipleDims) {
  TensorShape shape;
  shape.dims = {2, 3, 4};
  EXPECT_EQ(shape.element_count(), 24);
  EXPECT_EQ(shape.rank(), 3);
}

TEST(TensorShapeTest, DimWithOne) {
  TensorShape shape;
  shape.dims = {1, 5, 1};
  EXPECT_EQ(shape.element_count(), 5);
}

// ========================================================================
// Tensor::element_size — all element types
// ========================================================================

TEST(TensorElementSizeTest, Float32) {
  EXPECT_EQ(Tensor::element_size(ElementType::kFloat32), 4u);
}

TEST(TensorElementSizeTest, Float16) {
  EXPECT_EQ(Tensor::element_size(ElementType::kFloat16), 2u);
}

TEST(TensorElementSizeTest, BFloat16) {
  EXPECT_EQ(Tensor::element_size(ElementType::kBFloat16), 2u);
}

TEST(TensorElementSizeTest, Int8) {
  EXPECT_EQ(Tensor::element_size(ElementType::kInt8), 1u);
}

TEST(TensorElementSizeTest, UInt8) {
  EXPECT_EQ(Tensor::element_size(ElementType::kUInt8), 1u);
}

TEST(TensorElementSizeTest, Int16) {
  EXPECT_EQ(Tensor::element_size(ElementType::kInt16), 2u);
}

TEST(TensorElementSizeTest, UInt16) {
  EXPECT_EQ(Tensor::element_size(ElementType::kUInt16), 2u);
}

TEST(TensorElementSizeTest, Int32) {
  EXPECT_EQ(Tensor::element_size(ElementType::kInt32), 4u);
}

TEST(TensorElementSizeTest, UInt32) {
  EXPECT_EQ(Tensor::element_size(ElementType::kUInt32), 4u);
}

TEST(TensorElementSizeTest, Int64) {
  EXPECT_EQ(Tensor::element_size(ElementType::kInt64), 8u);
}

TEST(TensorElementSizeTest, UInt64) {
  EXPECT_EQ(Tensor::element_size(ElementType::kUInt64), 8u);
}

TEST(TensorElementSizeTest, Bool) {
  EXPECT_EQ(Tensor::element_size(ElementType::kBool), 1u);
}

TEST(TensorElementSizeTest, String) {
  // Variable-length — returns 0
  EXPECT_EQ(Tensor::element_size(ElementType::kString), 0u);
}

TEST(TensorElementSizeTest, Float64) {
  EXPECT_EQ(Tensor::element_size(ElementType::kFloat64), 8u);
}

// ========================================================================
// Tensor constructor — with data
// ========================================================================

TEST(TensorTest, ConstructWithData) {
  std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
  TensorShape shape;
  shape.dims = {2, 3};

  Tensor tensor(ElementType::kFloat32, shape, data.data(), data.size() * sizeof(float));

  EXPECT_EQ(tensor.element_type(), ElementType::kFloat32);
  EXPECT_EQ(tensor.shape().dims, (std::vector<int64_t>{2, 3}));
  EXPECT_EQ(tensor.element_count(), 6);
  EXPECT_EQ(tensor.data_size(), 24u);  // 6 * 4 bytes
  ASSERT_NE(tensor.data(), nullptr);

  // Verify the data was copied
  const auto* read = static_cast<const float*>(tensor.data());
  EXPECT_FLOAT_EQ(read[0], 1.0f);
  EXPECT_FLOAT_EQ(read[5], 6.0f);
}

TEST(TensorTest, ConstructWithShapeOnly_ZeroInitialized) {
  TensorShape shape;
  shape.dims = {3, 2};

  Tensor tensor(ElementType::kInt32, shape);

  EXPECT_EQ(tensor.element_type(), ElementType::kInt32);
  EXPECT_EQ(tensor.element_count(), 6);
  EXPECT_EQ(tensor.data_size(), 24u);  // 6 * 4 bytes

  // Buffer should be zero-initialized
  const auto* read = static_cast<const int32_t*>(tensor.data());
  for (int i = 0; i < 6; ++i) {
    EXPECT_EQ(read[i], 0);
  }
}

TEST(TensorTest, MutableDataCanBeWritten) {
  TensorShape shape;
  shape.dims = {4};

  Tensor tensor(ElementType::kFloat32, shape);

  auto* write = static_cast<float*>(tensor.mutable_data());
  write[0] = 42.0f;
  write[3] = 99.0f;

  const auto* read = static_cast<const float*>(tensor.data());
  EXPECT_FLOAT_EQ(read[0], 42.0f);
  EXPECT_FLOAT_EQ(read[3], 99.0f);
}

TEST(TensorTest, MoveConstructor) {
  TensorShape shape;
  shape.dims = {2};

  Tensor t1(ElementType::kFloat32, shape);
  auto* original_data = t1.data();

  Tensor t2(std::move(t1));
  EXPECT_EQ(t2.element_type(), ElementType::kFloat32);
  EXPECT_EQ(t2.data(), original_data);
}

// ========================================================================
// InferenceResult::Get
// ========================================================================

TEST(InferenceResultTest, GetByName_Found) {
  InferenceResult result;
  result.output_names = {"output_0", "output_1"};

  TensorShape s0;
  s0.dims = {2};
  TensorShape s1;
  s1.dims = {3};
  result.output_tensors.emplace_back(ElementType::kFloat32, s0);
  result.output_tensors.emplace_back(ElementType::kInt32, s1);

  const Tensor* t = result.Get("output_1");
  ASSERT_NE(t, nullptr);
  EXPECT_EQ(t->element_type(), ElementType::kInt32);
  EXPECT_EQ(t->element_count(), 3);
}

TEST(InferenceResultTest, GetByName_NotFound) {
  InferenceResult result;
  result.output_names = {"output_0"};

  TensorShape s;
  s.dims = {1};
  result.output_tensors.emplace_back(ElementType::kFloat32, s);

  EXPECT_EQ(result.Get("nonexistent"), nullptr);
}

TEST(InferenceResultTest, GetByName_EmptyResult) {
  InferenceResult result;
  EXPECT_EQ(result.Get("anything"), nullptr);
}

TEST(InferenceResultTest, GetByName_FirstNameMatch) {
  InferenceResult result;
  result.output_names = {"output_0"};

  TensorShape s;
  s.dims = {5};
  result.output_tensors.emplace_back(ElementType::kFloat64, s);

  const Tensor* t = result.Get("output_0");
  ASSERT_NE(t, nullptr);
  EXPECT_EQ(t->element_type(), ElementType::kFloat64);
}

TEST(InferenceResultTest, GetByName_NameIndexMismatch) {
  // Edge case: more names than tensors
  InferenceResult result;
  result.output_names = {"a", "b", "c"};

  TensorShape s;
  s.dims = {1};
  result.output_tensors.emplace_back(ElementType::kFloat32, s);

  // "a" at index 0 — tensor exists → valid
  EXPECT_NE(result.Get("a"), nullptr);

  // "b" at index 1 — no tensor at index 1 → nullptr
  EXPECT_EQ(result.Get("b"), nullptr);

  // "c" at index 2 — no tensor at index 2 → nullptr
  EXPECT_EQ(result.Get("c"), nullptr);
}
