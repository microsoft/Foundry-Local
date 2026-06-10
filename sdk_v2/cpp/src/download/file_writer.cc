// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "download/file_writer.h"
#include "exception.h"

#include <foundry_local/foundry_local_c.h>

#include <fstream>
#include <mutex>
#include <string>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#endif

namespace fl {

namespace {

namespace fs = std::filesystem;

/// Ensure the data file exists at exactly `expected_size`. Skips truncation
/// if the file is already at that size — the resume path relies on this.
void EnsureFileExistsAtSize(const fs::path& path, int64_t expected_size) {
  std::error_code ec;
  auto cur_size = fs::file_size(path, ec);
  if (!ec) {
    if (cur_size == static_cast<uintmax_t>(expected_size)) {
      return;
    }
    // File exists but is the wrong size — fall through to recreate.
  } else if (ec != std::errc::no_such_file_or_directory) {
    // Some other stat error (permission, transient NFS hiccup, AV scanner
    // holding a handle, etc.). Don't blow away a potentially-intact file
    // just because we couldn't read its size; surface the error instead so
    // the caller can retry and the existing on-disk progress is preserved.
    FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
             "failed to stat blob file: " + path.string() + " (" + ec.message() + ")");
  }

  std::ofstream f(path, std::ios::binary);
  if (!f.is_open()) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
             "failed to open blob file for pre-allocation: " + path.string());
  }
  if (expected_size > 0) {
    f.seekp(expected_size - 1);
    f.put('\0');
  }
  f.close();
  if (f.fail()) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
             "failed to pre-allocate blob file: " + path.string() +
                 " (size=" + std::to_string(expected_size) + ")");
  }
}

#ifdef _WIN32

class WindowsPositionalFileWriter : public IFileWriter {
 public:
  ~WindowsPositionalFileWriter() override { Close(); }

  void Open(const fs::path& path, int64_t expected_size) override {
    EnsureFileExistsAtSize(path, expected_size);
    // FILE_SHARE_READ | FILE_SHARE_WRITE so the lock file / other tools can
    // peek at the partial file without us erroring; positional WriteFile is
    // safe regardless of share mode.
    handle_ = ::CreateFileW(path.wstring().c_str(), GENERIC_READ | GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle_ == INVALID_HANDLE_VALUE) {
      FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
               "PositionalFileWriter open failed for " + path.string() +
                   " (Win32 err " + std::to_string(::GetLastError()) + ")");
    }
  }

  void WriteAt(int64_t offset, const uint8_t* data, size_t len) override {
    // Concurrent WriteFile calls with distinct OVERLAPPED offsets on the same
    // handle are safe for non-overlapping ranges; the kernel orders them.
    while (len > 0) {
      OVERLAPPED ov{};
      ov.Offset = static_cast<DWORD>(static_cast<uint64_t>(offset) & 0xFFFFFFFFULL);
      ov.OffsetHigh = static_cast<DWORD>((static_cast<uint64_t>(offset) >> 32) & 0xFFFFFFFFULL);
      DWORD to_write = static_cast<DWORD>(len > 0x7FFFFFFFu ? 0x7FFFFFFFu : len);
      DWORD written = 0;
      if (!::WriteFile(handle_, data, to_write, &written, &ov)) {
        FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
                 "PositionalFileWriter write failed at offset " + std::to_string(offset) +
                     " (Win32 err " + std::to_string(::GetLastError()) + ")");
      }
      if (written == 0) {
        FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
                 "PositionalFileWriter short write at offset " + std::to_string(offset));
      }
      offset += static_cast<int64_t>(written);
      data += written;
      len -= written;
    }
  }

  void Close() override {
    if (handle_ != INVALID_HANDLE_VALUE) {
      ::CloseHandle(handle_);
      handle_ = INVALID_HANDLE_VALUE;
    }
  }

 private:
  HANDLE handle_ = INVALID_HANDLE_VALUE;
};

#else  // POSIX

class PosixPositionalFileWriter : public IFileWriter {
 public:
  ~PosixPositionalFileWriter() override { Close(); }

  void Open(const fs::path& path, int64_t expected_size) override {
    EnsureFileExistsAtSize(path, expected_size);
    fd_ = ::open(path.c_str(), O_RDWR | O_CLOEXEC);
    if (fd_ < 0) {
      FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
               "PositionalFileWriter open failed for " + path.string() +
                   " (errno " + std::to_string(errno) + ")");
    }
  }

  void WriteAt(int64_t offset, const uint8_t* data, size_t len) override {
    while (len > 0) {
      ssize_t n = ::pwrite(fd_, data, len, static_cast<off_t>(offset));
      if (n < 0) {
        if (errno == EINTR) continue;
        FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
                 "PositionalFileWriter pwrite failed at offset " + std::to_string(offset) +
                     " (errno " + std::to_string(errno) + ")");
      }
      if (n == 0) {
        FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
                 "PositionalFileWriter short pwrite at offset " + std::to_string(offset));
      }
      offset += n;
      data += n;
      len -= static_cast<size_t>(n);
    }
  }

  void Close() override {
    if (fd_ >= 0) {
      ::close(fd_);
      fd_ = -1;
    }
  }

 private:
  int fd_ = -1;
};

#endif

class MutexFstreamFileWriter : public IFileWriter {
 public:
  ~MutexFstreamFileWriter() override { Close(); }

  void Open(const fs::path& path, int64_t expected_size) override {
    EnsureFileExistsAtSize(path, expected_size);
    file_.open(path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file_.is_open()) {
      FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
               "MutexFstreamFileWriter open failed for " + path.string());
    }
  }

  void WriteAt(int64_t offset, const uint8_t* data, size_t len) override {
    std::lock_guard<std::mutex> lock(mutex_);
    // Clear any sticky failbit from a prior call so this write's diagnostic
    // reflects what actually went wrong here, not a stale earlier failure.
    file_.clear();
    file_.seekp(offset);
    file_.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(len));
    if (file_.fail()) {
      FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
               "MutexFstreamFileWriter write failed at offset " + std::to_string(offset));
    }
  }

  void Close() override {
    if (file_.is_open()) {
      file_.close();
    }
  }

 private:
  std::fstream file_;
  std::mutex mutex_;
};

}  // namespace

std::unique_ptr<IFileWriter> MakePositionalFileWriter() {
#ifdef _WIN32
  return std::make_unique<WindowsPositionalFileWriter>();
#else
  return std::make_unique<PosixPositionalFileWriter>();
#endif
}

std::unique_ptr<IFileWriter> MakeMutexFstreamFileWriter() {
  return std::make_unique<MutexFstreamFileWriter>();
}

}  // namespace fl
