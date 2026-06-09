// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>

namespace fl {

/// Thread-safe positional writer for blob downloads.
///
/// Workers in a single download claim disjoint chunks, so concurrent
/// `WriteAt` calls always target non-overlapping byte ranges. An
/// implementation may serialize internally (e.g. via a mutex) or rely on the
/// OS to allow lock-free concurrent positional writes — the contract is the
/// same either way.
class IFileWriter {
 public:
  virtual ~IFileWriter() = default;

  /// Make `path` exist at exactly `expected_size` bytes. If the file already
  /// exists at that size, leave its contents intact (so the resume path can
  /// pick up where it left off). Called once before the first `WriteAt`.
  virtual void Open(const std::filesystem::path& path, int64_t expected_size) = 0;

  /// Write `len` bytes from `data` starting at byte offset `offset`.
  /// Thread-safe across overlapping or disjoint ranges — concurrent calls to
  /// disjoint ranges complete without coordination from the caller.
  virtual void WriteAt(int64_t offset, const uint8_t* data, size_t len) = 0;

  /// Release the underlying OS handle. Implicitly called by the destructor.
  virtual void Close() = 0;
};

/// Backed by `pwrite` (POSIX) or `WriteFile`+`OVERLAPPED` (Windows). Concurrent
/// `WriteAt` calls to disjoint ranges proceed in parallel — no internal
/// mutex. The recommended default.
std::unique_ptr<IFileWriter> MakePositionalFileWriter();

/// Backed by a single `std::fstream` guarded by an internal mutex. Provided
/// for comparison with `MakePositionalFileWriter` and as a portable fallback
/// if a platform's positional-write semantics ever change.
std::unique_ptr<IFileWriter> MakeMutexFstreamFileWriter();

}  // namespace fl
