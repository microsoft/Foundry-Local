// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "util/file_lock.h"

#include <chrono>
#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace fl {

FileLock::FileLock(const std::filesystem::path& lock_file_path, int timeout_ms) {
  // Ensure parent directory exists
  auto parent = lock_file_path.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent);
  }

  auto start = std::chrono::steady_clock::now();
  auto deadline = start + std::chrono::milliseconds(timeout_ms);

  // Retry loop — another process may hold the lock
  while (true) {
    handle_ = CreateFileW(lock_file_path.wstring().c_str(),
                          GENERIC_READ | GENERIC_WRITE,
                          0,  // exclusive — no sharing
                          nullptr,
                          OPEN_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL,
                          nullptr);

    if (handle_ != INVALID_HANDLE_VALUE) {
      break;
    }

    if (std::chrono::steady_clock::now() >= deadline) {
      throw std::runtime_error("FileLock: timeout acquiring lock on " + lock_file_path.string());
    }

    Sleep(500);  // retry every 500ms, matching C# behavior
  }
}

FileLock::~FileLock() {
  if (handle_ && handle_ != INVALID_HANDLE_VALUE) {
    CloseHandle(static_cast<HANDLE>(handle_));
  }
}

}  // namespace fl

#else  // POSIX

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include <thread>

namespace fl {

FileLock::FileLock(const std::filesystem::path& lock_file_path, int timeout_ms) {
  auto parent = lock_file_path.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent);
  }

  fd_ = open(lock_file_path.c_str(), O_RDWR | O_CREAT, 0666);
  if (fd_ < 0) {
    throw std::runtime_error("FileLock: failed to open " + lock_file_path.string());
  }

  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

  while (flock(fd_, LOCK_EX | LOCK_NB) != 0) {
    if (std::chrono::steady_clock::now() >= deadline) {
      close(fd_);
      fd_ = -1;
      throw std::runtime_error("FileLock: timeout acquiring lock on " + lock_file_path.string());
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
}

FileLock::~FileLock() {
  if (fd_ >= 0) {
    flock(fd_, LOCK_UN);
    close(fd_);
  }
}

}  // namespace fl

#endif
