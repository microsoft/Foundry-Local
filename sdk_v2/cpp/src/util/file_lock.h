// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <filesystem>
#include <string>

namespace fl {

/// RAII cross-process file lock. Uses LockFileEx (Windows) / flock (POSIX).
/// The lock file is created if it doesn't exist.
/// Throws on timeout or system error.
class FileLock {
 public:
  /// Acquires the lock, blocking up to timeout_ms milliseconds.
  /// @param lock_file_path  Path to the lock file to create/open.
  /// @param timeout_ms  Maximum time to wait for the lock (default: 10 minutes).
  explicit FileLock(const std::filesystem::path& lock_file_path, int timeout_ms = 600000);
  ~FileLock();

  // Non-copyable, non-movable
  FileLock(const FileLock&) = delete;
  FileLock& operator=(const FileLock&) = delete;

 private:
#ifdef _WIN32
  void* handle_ = nullptr;  // HANDLE, forward-declared to avoid windows.h in header
#else
  int fd_ = -1;
#endif
};

}  // namespace fl
