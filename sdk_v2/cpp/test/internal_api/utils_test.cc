// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Tests for utility functions in utils.h.
//
#include "utils.h"

#include "logger.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

using namespace fl;

namespace {

struct LogEntry {
  LogLevel level;
  std::string message;
};

class RecordingLogger : public ILogger {
 public:
  void Log(LogLevel level, std::string_view message) override {
    entries.push_back(LogEntry{level, std::string(message)});
  }

  std::vector<LogEntry> entries;
};

}  // namespace

// ========================================================================
// SplitModelNameAndVersion tests
// ========================================================================

TEST(SplitModelNameAndVersionTest, NameWithNumericVersion) {
  auto [name, version] = Utils::SplitModelNameAndVersion("model-name:3");
  EXPECT_EQ(name, "model-name");
  EXPECT_EQ(version, 3);
}

TEST(SplitModelNameAndVersionTest, NameWithVersionZero) {
  auto [name, version] = Utils::SplitModelNameAndVersion("model-name:0");
  EXPECT_EQ(name, "model-name");
  EXPECT_EQ(version, 0);
}

TEST(SplitModelNameAndVersionTest, NameWithoutVersion) {
  auto [name, version] = Utils::SplitModelNameAndVersion("model-name");
  EXPECT_EQ(name, "model-name");
  EXPECT_EQ(version, 0);
}

TEST(SplitModelNameAndVersionTest, MultipleColonsUsesLast) {
  // rfind should find the last colon
  auto [name, version] = Utils::SplitModelNameAndVersion("model:name:3");
  EXPECT_EQ(name, "model:name");
  EXPECT_EQ(version, 3);
}

TEST(SplitModelNameAndVersionTest, NonNumericSuffixReturnsWholeString) {
  auto [name, version] = Utils::SplitModelNameAndVersion("model-name:abc");
  EXPECT_EQ(name, "model-name:abc");
  EXPECT_EQ(version, 0);
}

TEST(SplitModelNameAndVersionTest, EmptySuffixAfterColon) {
  // Colon at the last position — pos == model_id.size() - 1
  auto [name, version] = Utils::SplitModelNameAndVersion("model-name:");
  EXPECT_EQ(name, "model-name:");
  EXPECT_EQ(version, 0);
}

TEST(SplitModelNameAndVersionTest, ColonAtPositionZero) {
  // pos == 0 — the implementation returns the whole string
  auto [name, version] = Utils::SplitModelNameAndVersion(":3");
  EXPECT_EQ(name, ":3");
  EXPECT_EQ(version, 0);
}

TEST(SplitModelNameAndVersionTest, EmptyString) {
  auto [name, version] = Utils::SplitModelNameAndVersion("");
  EXPECT_EQ(name, "");
  EXPECT_EQ(version, 0);
}

// ========================================================================
// RemoveDirectoryRecursive tests
// ========================================================================

class RemoveDirectoryRecursiveTest : public ::testing::Test {
 protected:
  std::filesystem::path temp_dir_;

  void SetUp() override {
    temp_dir_ = std::filesystem::temp_directory_path() / ("fl_remove_test_" + std::to_string(GetPid()));
    std::filesystem::create_directories(temp_dir_);
  }

  void TearDown() override {
    std::filesystem::remove_all(temp_dir_);
  }

#ifdef _WIN32
  static DWORD GetPid() { return ::GetCurrentProcessId(); }
#else
  static pid_t GetPid() { return ::getpid(); }
#endif
};

TEST_F(RemoveDirectoryRecursiveTest, RemovesEmptyDirectory) {
  auto target = temp_dir_ / "empty_dir";
  std::filesystem::create_directories(target);
  ASSERT_TRUE(std::filesystem::exists(target));

  EXPECT_TRUE(Utils::RemoveDirectoryRecursive(target.string()));
  EXPECT_FALSE(std::filesystem::exists(target));
}

TEST_F(RemoveDirectoryRecursiveTest, RemovesDirectoryWithFiles) {
  auto target = temp_dir_ / "dir_with_files";
  std::filesystem::create_directories(target);
  std::ofstream(target / "file1.txt") << "content1";
  std::ofstream(target / "file2.txt") << "content2";
  ASSERT_TRUE(std::filesystem::exists(target / "file1.txt"));

  EXPECT_TRUE(Utils::RemoveDirectoryRecursive(target.string()));
  EXPECT_FALSE(std::filesystem::exists(target));
}

TEST_F(RemoveDirectoryRecursiveTest, RemovesNestedDirectories) {
  auto target = temp_dir_ / "nested";
  std::filesystem::create_directories(target / "sub1" / "sub2");
  std::ofstream(target / "root.txt") << "root";
  std::ofstream(target / "sub1" / "mid.txt") << "mid";
  std::ofstream(target / "sub1" / "sub2" / "deep.txt") << "deep";

  EXPECT_TRUE(Utils::RemoveDirectoryRecursive(target.string()));
  EXPECT_FALSE(std::filesystem::exists(target));
}

TEST_F(RemoveDirectoryRecursiveTest, EmptyPathReturnsFalse) {
  EXPECT_FALSE(Utils::RemoveDirectoryRecursive(""));
}

TEST_F(RemoveDirectoryRecursiveTest, NonExistentPathFails) {
  auto target = temp_dir_ / "does_not_exist";
  EXPECT_FALSE(Utils::RemoveDirectoryRecursive(target.string()));
}

TEST(UtilsTest, LogAndThrowWithLocationLogsAndThrowsFormattedMessage) {
  RecordingLogger logger;
  CodeLocation location("C:/repo/src/worker.cc", 44, "Worker",
                        {"frame_alpha", "frame_beta"});

  bool caught = false;
  try {
    Utils::LogAndThrow(logger, location, "unable to continue", FOUNDRY_LOCAL_ERROR_INTERNAL);
  } catch (const Exception& ex) {
    caught = true;
    ASSERT_EQ(logger.entries.size(), 1u);
    EXPECT_EQ(logger.entries[0].level, LogLevel::Error);
    EXPECT_EQ(logger.entries[0].message, ex.what());
    EXPECT_NE(logger.entries[0].message.find("worker.cc:44 Worker unable to continue"), std::string::npos);
    EXPECT_NE(logger.entries[0].message.find("Stacktrace:"), std::string::npos);
    EXPECT_NE(logger.entries[0].message.find("  frame_alpha"), std::string::npos);
    EXPECT_NE(logger.entries[0].message.find("  frame_beta"), std::string::npos);
  }
  EXPECT_TRUE(caught) << "expected fl::Exception";
}
