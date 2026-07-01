// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
// Shared temp directory / path helpers for tests. CTest (gtest_discover_tests)
// launches a separate process per test, so temp names embed the pid plus a
// per-process counter and never collide across concurrent test processes.
#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <process.h>  // _getpid
#else
#include <unistd.h>  // getpid
#endif

namespace fl::test {

/// Current process id. process.h is used instead of windows.h so callers that use
/// std::min/std::max aren't broken by its macros.
inline long CurrentPid() {
#ifdef _WIN32
  return ::_getpid();
#else
  return static_cast<long>(::getpid());
#endif
}

/// Build a unique path under the system temp directory as `<prefix><pid>_<counter>`. The pid
/// separates concurrent test processes and the per-process atomic counter separates callers
/// within one process, so no two live temp paths collide — no randomness required.
inline std::filesystem::path MakeUniqueTempPath(const std::string& prefix) {
  static std::atomic<uint64_t> counter{0};
  return std::filesystem::temp_directory_path() /
         (prefix + std::to_string(CurrentPid()) + "_" +
          std::to_string(counter.fetch_add(1, std::memory_order_relaxed)));
}

/// RAII temporary directory for test isolation. The name comes from MakeUniqueTempPath, so it is
/// unique both across the separate processes CTest launches per test and across multiple TempDirs
/// within one test. create_directory must succeed — the directory must not already exist — so a
/// residual collision (e.g. a directory leaked by an earlier process that reused this pid) advances
/// to the next name and retries instead of silently sharing an existing directory.
class TempDir {
 public:
  explicit TempDir(const std::string& prefix = "fl_test_") {
    while (true) {
      auto candidate = MakeUniqueTempPath(prefix);
      std::error_code ec;
      if (std::filesystem::create_directory(candidate, ec)) {
        path_ = std::move(candidate);
        return;
      }
      if (ec) {
        throw std::runtime_error("TempDir: failed to create '" + candidate.string() + "': " +
                                 ec.message());
      }
      // candidate already existed — try the next name.
    }
  }

  ~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }

  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;

  const std::filesystem::path& path() const { return path_; }
  std::string string() const { return path_.string(); }

 private:
  std::filesystem::path path_;
};

/// RAII unique temporary file path. The path is not created here — callers create the file — and
/// it is removed on destruction so a flaky test never leaks files into the system temp dir.
class TempPath {
 public:
  explicit TempPath(const std::string& prefix = "fl_test_") : path_(MakeUniqueTempPath(prefix)) {}

  ~TempPath() {
    std::error_code ec;
    std::filesystem::remove(path_, ec);
  }

  TempPath(const TempPath&) = delete;
  TempPath& operator=(const TempPath&) = delete;

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

}  // namespace fl::test
