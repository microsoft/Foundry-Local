// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "download/cross_process_file_lock.h"
#include "exception.h"
#include "logger.h"

#include <foundry_local/foundry_local_c.h>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <process.h>
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace fl {

namespace {

constexpr const char* kLockFileName = ".download.lock";

/// `PID:<pid>,Time:<iso8601-utc>\n` — mirrors what C# writes
/// (CrossProcessFileLock.cs:68) so the lock file is recognizable across SDKs.
std::string FormatProcessInfo() {
#ifdef _WIN32
  auto pid = static_cast<unsigned long>(_getpid());
#else
  auto pid = static_cast<unsigned long>(getpid());
#endif
  auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm tm{};
#ifdef _WIN32
  gmtime_s(&tm, &t);
#else
  gmtime_r(&t, &tm);
#endif
  std::ostringstream oss;
  oss << "PID:" << pid << ",Time:" << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ") << '\n';
  return oss.str();
}

}  // namespace

// Platform-specific resource handle. The destructor here is the only thing
// that releases the lock; CrossProcessFileLock's destructor is defaulted.
#ifdef _WIN32
struct CrossProcessFileLock::State {
  HANDLE handle;
  ~State() {
    if (handle != INVALID_HANDLE_VALUE) {
      // FILE_FLAG_DELETE_ON_CLOSE removes the file when the last handle closes.
      CloseHandle(handle);
    }
  }
};
#else
struct CrossProcessFileLock::State {
  int fd;
  std::filesystem::path path;
  ~State() {
    if (fd >= 0) {
      // Unlink before close so the file disappears at the same instant the
      // lock releases; a concurrent acquirer simply recreates it.
      ::unlink(path.c_str());
      ::close(fd);
    }
  }
};
#endif

CrossProcessFileLock::CrossProcessFileLock(std::filesystem::path path,
                                           std::unique_ptr<State> state,
                                           ILogger* logger)
    : path_(std::move(path)), state_(std::move(state)), logger_(logger) {}

CrossProcessFileLock::~CrossProcessFileLock() {
  // Release the OS handle first so the "released" log message is accurate.
  state_.reset();
  if (logger_) {
    logger_->Log(LogLevel::Debug, "CrossProcessFileLock released: " + path_.string());
  }
}

std::unique_ptr<CrossProcessFileLock> CrossProcessFileLock::TryAcquireForDirectory(
    const std::filesystem::path& directory, ILogger* logger) {
  std::error_code ec;
  std::filesystem::create_directories(directory, ec);
  // Best-effort: if create_directories failed, the platform open below will
  // surface a clearer error message.

  auto lock_path = directory / kLockFileName;
  std::unique_ptr<State> state;

#ifdef _WIN32
  // dwShareMode=0 blocks any other open (cross- and in-process) until this
  // handle closes. FILE_FLAG_DELETE_ON_CLOSE pairs OPEN_ALWAYS into a
  // self-cleaning lock that doesn't require unlink-then-close races.
  auto wide = lock_path.wstring();
  HANDLE handle = CreateFileW(wide.c_str(),
                              GENERIC_READ | GENERIC_WRITE,
                              0,
                              nullptr,
                              OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE,
                              nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    DWORD err = GetLastError();
    if (err == ERROR_SHARING_VIOLATION || err == ERROR_LOCK_VIOLATION || err == ERROR_ACCESS_DENIED) {
      // ACCESS_DENIED can surface on FILE_SHARE_NONE collisions when the
      // existing handle has narrower access rights — treat as contention.
      return nullptr;
    }
    FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
             "CreateFileW failed for lock '" + lock_path.string() +
                 "' (GetLastError=" + std::to_string(err) + ")");
  }

  auto info = FormatProcessInfo();
  DWORD written = 0;
  WriteFile(handle, info.data(), static_cast<DWORD>(info.size()), &written, nullptr);
  FlushFileBuffers(handle);

  state = std::unique_ptr<State>(new State{handle});
#else
  int fd = ::open(lock_path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0644);
  if (fd < 0) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
             "open failed for lock '" + lock_path.string() + "' (errno=" + std::to_string(errno) + ")");
  }
  if (::flock(fd, LOCK_EX | LOCK_NB) != 0) {
    int err = errno;
    ::close(fd);
    if (err == EWOULDBLOCK || err == EAGAIN) {
      return nullptr;
    }
    FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
             "flock failed for '" + lock_path.string() + "' (errno=" + std::to_string(err) + ")");
  }

  (void)::ftruncate(fd, 0);
  auto info = FormatProcessInfo();
  (void)::write(fd, info.data(), info.size());

  state = std::unique_ptr<State>(new State{fd, lock_path});
#endif

  if (logger) {
    logger->Log(LogLevel::Debug, "CrossProcessFileLock acquired: " + lock_path.string());
  }
  return std::unique_ptr<CrossProcessFileLock>(
      new CrossProcessFileLock(std::move(lock_path), std::move(state), logger));
}

std::unique_ptr<CrossProcessFileLock> WaitForLockForDirectory(
    const std::filesystem::path& directory,
    const CancellationPredicate& is_cancelled,
    ILogger* logger,
    std::chrono::milliseconds poll_interval,
    std::chrono::milliseconds timeout) {
  auto deadline = std::chrono::steady_clock::now() + timeout;
  // Poll cancellation in slices of at most 100 ms so a long poll interval
  // (1.25 s default) doesn't keep a cancelling caller waiting.
  constexpr std::chrono::milliseconds kCancelSlice{100};
  while (true) {
    if (is_cancelled && is_cancelled()) {
      FL_THROW(FOUNDRY_LOCAL_ERROR_OPERATION_CANCELLED, "lock acquisition cancelled");
    }
    auto lock = CrossProcessFileLock::TryAcquireForDirectory(directory, logger);
    if (lock) {
      return lock;
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
               "timed out waiting for cross-process download lock on '" + directory.string() + "'");
    }
    auto slice_end = std::chrono::steady_clock::now() + poll_interval;
    while (std::chrono::steady_clock::now() < slice_end) {
      if (is_cancelled && is_cancelled()) {
        FL_THROW(FOUNDRY_LOCAL_ERROR_OPERATION_CANCELLED, "lock acquisition cancelled");
      }
      std::this_thread::sleep_for(std::min(kCancelSlice, poll_interval));
    }
  }
}

}  // namespace fl
