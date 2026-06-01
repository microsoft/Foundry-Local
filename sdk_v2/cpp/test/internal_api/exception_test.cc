// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "exception.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace fl;

namespace {

void ThrowViaMacro() {
  FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL, "macro failure");
}

}  // namespace

TEST(ExceptionTest, LocationConstructorIncludesLocationAndMessage) {
  CodeLocation location("C:/repo/src/sample.cc", 27, "SampleFunction");

  Exception ex(location, "boom");

  std::string message = ex.what();
  EXPECT_NE(message.find("sample.cc:27 SampleFunction boom"), std::string::npos);
  EXPECT_EQ(message.find("Stacktrace:"), std::string::npos);
  EXPECT_EQ(ex.code(), FOUNDRY_LOCAL_ERROR_INTERNAL);
}

TEST(ExceptionTest, LocationConstructorIncludesSyntheticStacktraceFrames) {
  CodeLocation location("C:/repo/src/sample.cc", 28, "SampleFunction",
                        {"frame_one", "frame_two"});

  Exception ex(location, "boom");

  std::string message = ex.what();
  EXPECT_NE(message.find("sample.cc:28 SampleFunction boom"), std::string::npos);
  EXPECT_NE(message.find("Stacktrace:"), std::string::npos);
  EXPECT_NE(message.find("  frame_one"), std::string::npos);
  EXPECT_NE(message.find("  frame_two"), std::string::npos);
  EXPECT_EQ(ex.code(), FOUNDRY_LOCAL_ERROR_INTERNAL);
}

TEST(ExceptionTest, LocationConstructorPreservesErrorCode) {
  CodeLocation location("C:/repo/src/sample.cc", 29, "SampleFunction");

  Exception ex(location, "boom", FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT);

  EXPECT_EQ(ex.code(), FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT);
  EXPECT_NE(std::string(ex.what()).find("sample.cc:29 SampleFunction boom"), std::string::npos);
}

TEST(ExceptionTest, ThrowMacroIncludesCallSiteAndMessage) {
  try {
    ThrowViaMacro();
    FAIL() << "expected fl::Exception";
  } catch (const Exception& ex) {
    std::string message = ex.what();
    EXPECT_NE(message.find("exception_test.cc"), std::string::npos);
    EXPECT_NE(message.find("macro failure"), std::string::npos);
    EXPECT_EQ(ex.code(), FOUNDRY_LOCAL_ERROR_INTERNAL);
  }
}