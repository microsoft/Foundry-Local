// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
// Unit tests for zip-slip pre-validation in util/zip_extract.
#include "util/zip_extract.h"

#include <gtest/gtest.h>

namespace fl {

TEST(IsSafeArchiveEntryTest, AcceptsSimpleFilename) {
  EXPECT_TRUE(IsSafeArchiveEntry("file.bin"));
}

TEST(IsSafeArchiveEntryTest, AcceptsNestedRelativePath) {
  EXPECT_TRUE(IsSafeArchiveEntry("dir/sub/file.bin"));
}

TEST(IsSafeArchiveEntryTest, AcceptsBackslashRelativePath) {
  EXPECT_TRUE(IsSafeArchiveEntry("dir\\sub\\file.bin"));
}

TEST(IsSafeArchiveEntryTest, AcceptsEmpty) {
  EXPECT_TRUE(IsSafeArchiveEntry(""));
}

TEST(IsSafeArchiveEntryTest, AcceptsDotComponent) {
  // A single "." component is benign and commonly emitted by tar.
  EXPECT_TRUE(IsSafeArchiveEntry("./file.bin"));
}

TEST(IsSafeArchiveEntryTest, RejectsLeadingParent) {
  EXPECT_FALSE(IsSafeArchiveEntry("../escape.txt"));
}

TEST(IsSafeArchiveEntryTest, RejectsLeadingParentBackslash) {
  EXPECT_FALSE(IsSafeArchiveEntry("..\\escape.txt"));
}

TEST(IsSafeArchiveEntryTest, RejectsMidPathParent) {
  EXPECT_FALSE(IsSafeArchiveEntry("dir/../escape.txt"));
}

TEST(IsSafeArchiveEntryTest, RejectsTrailingParent) {
  EXPECT_FALSE(IsSafeArchiveEntry("dir/.."));
}

TEST(IsSafeArchiveEntryTest, RejectsDeepParentChain) {
  EXPECT_FALSE(IsSafeArchiveEntry("a/b/../../../etc/passwd"));
}

TEST(IsSafeArchiveEntryTest, RejectsAbsolutePosixPath) {
  EXPECT_FALSE(IsSafeArchiveEntry("/etc/passwd"));
}

TEST(IsSafeArchiveEntryTest, RejectsLeadingBackslash) {
  EXPECT_FALSE(IsSafeArchiveEntry("\\Windows\\System32"));
}

TEST(IsSafeArchiveEntryTest, RejectsWindowsDriveLetter) {
  EXPECT_FALSE(IsSafeArchiveEntry("C:\\Windows\\System32"));
}

TEST(IsSafeArchiveEntryTest, RejectsLowerCaseDriveLetter) {
  EXPECT_FALSE(IsSafeArchiveEntry("c:/Windows"));
}

TEST(IsSafeArchiveEntryTest, RejectsAnyColon) {
  // Defensive: archive entries should never legitimately contain ':'.
  EXPECT_FALSE(IsSafeArchiveEntry("dir/foo:bar"));
}

TEST(IsSafeArchiveEntryTest, AcceptsParentLikeFilename) {
  // ".." must be rejected as a component, but "..foo" is just a filename.
  EXPECT_TRUE(IsSafeArchiveEntry("..foo"));
  EXPECT_TRUE(IsSafeArchiveEntry("foo.."));
  EXPECT_TRUE(IsSafeArchiveEntry("dir/...hidden"));
}

}  // namespace fl
