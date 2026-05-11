// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Tests for the runtime library path storage used by the delay-load hook.

#include "util/runtime_library_path.h"

#include <gtest/gtest.h>

using namespace fl;

class RuntimeLibraryPathTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ResetRuntimeLibraryPathForTesting();
  }

  void TearDown() override {
    ResetRuntimeLibraryPathForTesting();
  }
};

TEST_F(RuntimeLibraryPathTest, DefaultIsEmpty) {
  EXPECT_TRUE(GetRuntimeLibraryPath().empty());
}

TEST_F(RuntimeLibraryPathTest, SetAndGet) {
  EXPECT_TRUE(SetRuntimeLibraryPath("C:\\ort\\libs"));
  EXPECT_EQ(GetRuntimeLibraryPath(), "C:\\ort\\libs");
}

TEST_F(RuntimeLibraryPathTest, SetOverwritesPrevious) {
  EXPECT_TRUE(SetRuntimeLibraryPath("/first/path"));
  EXPECT_TRUE(SetRuntimeLibraryPath("/second/path"));
  EXPECT_EQ(GetRuntimeLibraryPath(), "/second/path");
}

TEST_F(RuntimeLibraryPathTest, SetNullClearsPath) {
  EXPECT_TRUE(SetRuntimeLibraryPath("/some/path"));
  EXPECT_TRUE(SetRuntimeLibraryPath(nullptr));
  EXPECT_TRUE(GetRuntimeLibraryPath().empty());
}

TEST_F(RuntimeLibraryPathTest, SetEmptyStringClearsPath) {
  EXPECT_TRUE(SetRuntimeLibraryPath("/some/path"));
  EXPECT_TRUE(SetRuntimeLibraryPath(""));
  EXPECT_TRUE(GetRuntimeLibraryPath().empty());
}

TEST_F(RuntimeLibraryPathTest, MarkOrtLoadedPreventsSubsequentSet) {
  EXPECT_TRUE(SetRuntimeLibraryPath("/ort/dir"));
  MarkOrtLoaded();
  EXPECT_FALSE(SetRuntimeLibraryPath("/other/dir"));

  // Original path is preserved
  EXPECT_EQ(GetRuntimeLibraryPath(), "/ort/dir");
}

TEST_F(RuntimeLibraryPathTest, MarkOrtLoadedWithoutPriorSet) {
  MarkOrtLoaded();
  EXPECT_FALSE(SetRuntimeLibraryPath("/too/late"));
  EXPECT_TRUE(GetRuntimeLibraryPath().empty());
}

TEST_F(RuntimeLibraryPathTest, ResetAllowsSetAfterMarkLoaded) {
  MarkOrtLoaded();
  EXPECT_FALSE(SetRuntimeLibraryPath("/blocked"));

  ResetRuntimeLibraryPathForTesting();

  EXPECT_TRUE(SetRuntimeLibraryPath("/works/again"));
  EXPECT_EQ(GetRuntimeLibraryPath(), "/works/again");
}
