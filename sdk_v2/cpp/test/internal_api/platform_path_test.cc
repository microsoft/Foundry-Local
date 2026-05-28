// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
// Tests for the platform path canonicalization helpers in src/platform/path.h.
// Most cases target the Windows AppContainer fallback (see
// platform/windows/path.cc); the cross-platform smoke test runs everywhere.
#include "platform/path.h"

#ifdef _WIN32
#include "platform/windows/path_internal.h"
#endif

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

class PlatformPathFixture : public ::testing::Test {
 protected:
  void SetUp() override {
    base_dir_ = fs::temp_directory_path() /
                ("fl_platform_path_test_" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(base_dir_);
  }

  void TearDown() override {
    std::error_code ec;
    fs::remove_all(base_dir_, ec);
  }

  void CreateEmptyFile(const fs::path& path) { std::ofstream{path}; }

  fs::path base_dir_;
};

}  // namespace

// Cross-platform smoke test for the abstraction.
TEST_F(PlatformPathFixture, GetWeaklyCanonicalPath_ExistingDirectory) {
  fs::path canonical;
  std::string error;
  ASSERT_TRUE(fl::platform::GetWeaklyCanonicalPath(base_dir_, canonical, error)) << error;

  std::error_code ec;
  EXPECT_TRUE(fs::equivalent(base_dir_, canonical, ec)) << ec.message();
}

TEST_F(PlatformPathFixture, GetWeaklyCanonicalPath_NonExistentLeafPreserved) {
  const fs::path target = base_dir_ / "does_not_exist.bin";

  fs::path canonical;
  std::string error;
  ASSERT_TRUE(fl::platform::GetWeaklyCanonicalPath(target, canonical, error)) << error;

  EXPECT_EQ(canonical.filename(), fs::path{"does_not_exist.bin"});
}

#if defined(_WIN32)

// Direct tests for the Windows AppContainer fallback used by
// fl::platform::GetWeaklyCanonicalPath. The AppContainer trigger itself can't
// be reproduced in a unit test environment.

TEST_F(PlatformPathFixture, WeaklyCanonicalPathNtVolumeFallback_ExistingDirectory) {
  fs::path canonical;
  ASSERT_TRUE(fl::platform::internal::WeaklyCanonicalPathNtVolumeFallback(base_dir_, canonical));

  EXPECT_NE(canonical.wstring().find(L"\\\\?\\GLOBALROOT\\Device\\"), std::wstring::npos);

  std::error_code ec;
  EXPECT_TRUE(fs::exists(canonical, ec)) << "ec=" << ec.message();
}

TEST_F(PlatformPathFixture, WeaklyCanonicalPathNtVolumeFallback_ExistingFile) {
  CreateEmptyFile(base_dir_ / "data.bin");

  fs::path canonical;
  ASSERT_TRUE(
      fl::platform::internal::WeaklyCanonicalPathNtVolumeFallback(base_dir_ / "data.bin", canonical));

  EXPECT_NE(canonical.wstring().find(L"\\\\?\\GLOBALROOT\\Device\\"), std::wstring::npos);

  std::error_code ec;
  EXPECT_TRUE(fs::exists(canonical, ec)) << "ec=" << ec.message();
  EXPECT_TRUE(fs::is_regular_file(canonical, ec)) << "ec=" << ec.message();
}

TEST_F(PlatformPathFixture, WeaklyCanonicalPathNtVolumeFallback_NonExistentLeafLexicallyAppended) {
  const fs::path leaf{L"does_not_exist.bin"};
  fs::path canonical;
  ASSERT_TRUE(fl::platform::internal::WeaklyCanonicalPathNtVolumeFallback(base_dir_ / leaf, canonical));

  EXPECT_NE(canonical.wstring().find(L"\\\\?\\GLOBALROOT\\Device\\"), std::wstring::npos);
  EXPECT_EQ(canonical.filename(), leaf);

  // The canonicalized parent must be a path-component prefix of the result so
  // that the containment check in IsPathWithinDirectory still works.
  fs::path parent_canonical;
  ASSERT_TRUE(
      fl::platform::internal::WeaklyCanonicalPathNtVolumeFallback(base_dir_, parent_canonical));
  auto [parent_end, full_it] = std::mismatch(parent_canonical.begin(), parent_canonical.end(),
                                             canonical.begin(), canonical.end());
  EXPECT_EQ(parent_end, parent_canonical.end())
      << "parent: " << parent_canonical << " full: " << canonical;
}

TEST_F(PlatformPathFixture, WeaklyCanonicalPathNtVolumeFallback_NonExistentMiddleAndLeaf) {
  fs::path canonical;
  ASSERT_TRUE(fl::platform::internal::WeaklyCanonicalPathNtVolumeFallback(
      base_dir_ / L"missing_dir" / L"data.bin", canonical));

  EXPECT_NE(canonical.wstring().find(L"\\\\?\\GLOBALROOT\\Device\\"), std::wstring::npos);
  EXPECT_EQ(canonical.filename(), fs::path{L"data.bin"});
  EXPECT_EQ(canonical.parent_path().filename(), fs::path{L"missing_dir"});
}

TEST_F(PlatformPathFixture, WeaklyCanonicalPathNtVolumeFallback_AllNonExistentReturnsFalse) {
  // Synthetic absolute path on a non-existent volume. The fallback must return
  // false so the caller surfaces the original weakly_canonical error rather
  // than substituting an unverified path.
  const fs::path bogus{L"\\\\?\\Volume{00000000-0000-0000-0000-000000000000}\\nope\\data.bin"};
  fs::path canonical;
  EXPECT_FALSE(fl::platform::internal::WeaklyCanonicalPathNtVolumeFallback(bogus, canonical));
}

TEST_F(PlatformPathFixture, WeaklyCanonicalPathNtVolumeFallback_MatchesWeaklyCanonicalAtFile) {
  // Compare via fs::equivalent: the fallback returns the NT form
  // (\\?\GLOBALROOT\Device\HarddiskVolumeN\...) while weakly_canonical returns
  // the DOS form (C:\...), but both must point at the same file.
  CreateEmptyFile(base_dir_ / "compare.bin");
  const auto target = base_dir_ / "compare.bin";

  std::error_code ec;
  const auto reference = fs::weakly_canonical(target, ec);
  ASSERT_FALSE(ec) << ec.message();

  fs::path fallback;
  ASSERT_TRUE(fl::platform::internal::WeaklyCanonicalPathNtVolumeFallback(target, fallback));

  EXPECT_TRUE(fs::equivalent(reference, fallback, ec))
      << "reference=" << reference << " fallback=" << fallback << " ec=" << ec.message();
}

TEST_F(PlatformPathFixture, WeaklyCanonicalPathNtVolumeFallback_ResolvesDotDot) {
  fs::create_directories(base_dir_ / "sub_for_dotdot");

  fs::path canonical;
  ASSERT_TRUE(fl::platform::internal::WeaklyCanonicalPathNtVolumeFallback(
      base_dir_ / "sub_for_dotdot" / "..", canonical));

  fs::path base_canonical;
  ASSERT_TRUE(
      fl::platform::internal::WeaklyCanonicalPathNtVolumeFallback(base_dir_, base_canonical));

  std::error_code ec;
  EXPECT_TRUE(fs::equivalent(canonical, base_canonical, ec))
      << "canonical=" << canonical << " base=" << base_canonical << " ec=" << ec.message();
}

#endif  // _WIN32
