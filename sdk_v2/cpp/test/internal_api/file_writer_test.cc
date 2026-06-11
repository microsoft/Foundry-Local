// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Tests for the IFileWriter abstraction backing AzureBlobDownloader's chunked
// writes. Exercises both implementations (Positional / MutexFstream) through a
// parametrized fixture so every correctness assertion runs against both.
//
// The "PerfComparison" test prints wall-clock numbers for a representative
// download workload (32 threads, 64-way chunked streaming into a 256 MB file)
// so we can eyeball lock contention deltas without adding a separate
// microbenchmark binary. It is informational — its only EXPECT is that both
// runs complete and the file ends up at the right size.

#include "download/file_writer.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using namespace fl;

namespace {

class TempPath {
 public:
  TempPath() {
    auto base = fs::temp_directory_path();
    std::random_device rd;
    std::uniform_int_distribution<uint64_t> dist;
    path_ = base / ("file_writer_test_" + std::to_string(dist(rd)) + ".bin");
  }
  ~TempPath() {
    std::error_code ec;
    fs::remove(path_, ec);
  }
  const fs::path& path() const { return path_; }

 private:
  fs::path path_;
};

std::unique_ptr<IFileWriter> MakeWriter(const std::string& kind) {
  if (kind == "Positional") return MakePositionalFileWriter();
  if (kind == "MutexFstream") return MakeMutexFstreamFileWriter();
  ADD_FAILURE() << "unknown writer kind " << kind;
  return nullptr;
}

class FileWriterTest : public ::testing::TestWithParam<std::string> {};

}  // namespace

TEST_P(FileWriterTest, OpenCreatesFileAtRequestedSize) {
  TempPath p;
  auto w = MakeWriter(GetParam());
  ASSERT_NE(w, nullptr);
  w->Open(p.path(), 4096);
  w->Close();
  EXPECT_TRUE(fs::exists(p.path()));
  EXPECT_EQ(fs::file_size(p.path()), 4096u);
}

TEST_P(FileWriterTest, OpenPreservesExistingFileAtSameSize) {
  TempPath p;
  // Pre-write a sentinel byte the writer must NOT overwrite.
  {
    std::ofstream f(p.path(), std::ios::binary);
    f.seekp(1023);
    f.put('\0');
  }
  // Plant a known byte at offset 100.
  {
    std::fstream f(p.path(), std::ios::binary | std::ios::in | std::ios::out);
    f.seekp(100);
    f.put(static_cast<char>(0xAB));
  }

  auto w = MakeWriter(GetParam());
  ASSERT_NE(w, nullptr);
  w->Open(p.path(), 1024);  // same size -> must not truncate
  w->Close();

  // Sentinel byte should still be there.
  std::ifstream f(p.path(), std::ios::binary);
  f.seekg(100);
  int byte = f.get();
  EXPECT_EQ(byte, 0xAB);
}

TEST_P(FileWriterTest, OpenTruncatesIfSizeChanged) {
  TempPath p;
  {
    std::ofstream f(p.path(), std::ios::binary);
    f.seekp(100);
    f.put(static_cast<char>(0xCD));
  }
  EXPECT_EQ(fs::file_size(p.path()), 101u);

  auto w = MakeWriter(GetParam());
  ASSERT_NE(w, nullptr);
  w->Open(p.path(), 4096);
  w->Close();
  EXPECT_EQ(fs::file_size(p.path()), 4096u);
}

TEST_P(FileWriterTest, SingleThreadWriteAt) {
  TempPath p;
  auto w = MakeWriter(GetParam());
  ASSERT_NE(w, nullptr);
  w->Open(p.path(), 1024);

  std::vector<uint8_t> data(256, 0xEF);
  w->WriteAt(512, data.data(), data.size());
  w->Close();

  std::ifstream f(p.path(), std::ios::binary);
  std::vector<uint8_t> contents((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
  ASSERT_EQ(contents.size(), 1024u);
  for (size_t i = 512; i < 768; ++i) {
    EXPECT_EQ(contents[i], 0xEF) << "byte " << i;
  }
}

TEST_P(FileWriterTest, ConcurrentDisjointWritesProduceCorrectFile) {
  TempPath p;
  constexpr int kThreads = 8;
  constexpr int kRegionSize = 256 * 1024;  // 256 KB per thread
  constexpr int kPieceSize = 16 * 1024;    // 16 KB per WriteAt
  constexpr int64_t kTotalSize = int64_t{kThreads} * kRegionSize;
  static_assert(kRegionSize % kPieceSize == 0, "");

  auto w = MakeWriter(GetParam());
  ASSERT_NE(w, nullptr);
  w->Open(p.path(), kTotalSize);

  std::atomic<int> started{0};
  std::vector<std::thread> workers;
  workers.reserve(kThreads);
  for (int t = 0; t < kThreads; ++t) {
    workers.emplace_back([&, t]() {
      std::vector<uint8_t> piece(kPieceSize, static_cast<uint8_t>(t + 1));
      started.fetch_add(1);
      while (started.load() < kThreads) {
        // tiny spin to encourage concurrent dispatch
      }
      const int64_t base = int64_t{t} * kRegionSize;
      for (int i = 0; i < kRegionSize / kPieceSize; ++i) {
        w->WriteAt(base + int64_t{i} * kPieceSize, piece.data(), piece.size());
      }
    });
  }
  for (auto& th : workers) th.join();
  w->Close();

  std::ifstream f(p.path(), std::ios::binary);
  std::vector<uint8_t> contents((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
  ASSERT_EQ(contents.size(), static_cast<size_t>(kTotalSize));
  for (int t = 0; t < kThreads; ++t) {
    const uint8_t expected = static_cast<uint8_t>(t + 1);
    for (int64_t i = 0; i < kRegionSize; ++i) {
      const auto idx = static_cast<size_t>(int64_t{t} * kRegionSize + i);
      if (contents[idx] != expected) {
        FAIL() << "mismatch at offset " << idx << " (thread " << t << ", expected "
               << static_cast<int>(expected) << ", got " << static_cast<int>(contents[idx]) << ")";
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(WriterImpls, FileWriterTest,
                         ::testing::Values("Positional", "MutexFstream"),
                         [](const ::testing::TestParamInfo<std::string>& info) {
                           return info.param;
                         });

// ---------------------------------------------------------------------------
// Perf comparison: print wall-clock for both writer kinds against a workload
// that mirrors AzureBlobDownloader (32 workers each streaming 8 chunks of 2 MB
// in 64 KB sink pieces). Run direct:
//     foundry_local_tests --gtest_filter=FileWriterPerfComparison.*
// ---------------------------------------------------------------------------

namespace {

struct PerfResult {
  std::string kind;
  int64_t elapsed_ms;
  double mb_per_sec;
};

PerfResult RunChunkedWorkload(const std::string& kind) {
  constexpr int kThreads = 32;
  constexpr int kChunksPerThread = 8;
  constexpr int kChunkSize = 2 * 1024 * 1024;  // 2 MB chunk like the downloader
  constexpr int kPieceSize = 64 * 1024;        // 64 KB scratch like the downloader
  constexpr int64_t kTotalSize = int64_t{kThreads} * kChunksPerThread * kChunkSize;
  static_assert(kChunkSize % kPieceSize == 0, "");

  TempPath p;
  auto w = MakeWriter(kind);
  if (!w) {
    ADD_FAILURE() << "MakeWriter returned null for " << kind;
    return {kind, 0, 0.0};
  }
  w->Open(p.path(), kTotalSize);

  std::atomic<int> next_chunk{0};
  const int total_chunks = kThreads * kChunksPerThread;

  auto start = std::chrono::steady_clock::now();
  std::vector<std::thread> workers;
  workers.reserve(kThreads);
  for (int t = 0; t < kThreads; ++t) {
    workers.emplace_back([&, t]() {
      std::vector<uint8_t> scratch(kPieceSize, static_cast<uint8_t>(t & 0xFF));
      while (true) {
        int i = next_chunk.fetch_add(1, std::memory_order_relaxed);
        if (i >= total_chunks) return;
        const int64_t chunk_off = int64_t{i} * kChunkSize;
        for (int pos = 0; pos < kChunkSize; pos += kPieceSize) {
          w->WriteAt(chunk_off + pos, scratch.data(), kPieceSize);
        }
      }
    });
  }
  for (auto& th : workers) th.join();
  w->Close();
  auto elapsed = std::chrono::steady_clock::now() - start;
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

  EXPECT_EQ(fs::file_size(p.path()), static_cast<uintmax_t>(kTotalSize));

  double mb_per_sec =
      static_cast<double>(kTotalSize) / (1024.0 * 1024.0) / (static_cast<double>(ms) / 1000.0);
  return {kind, ms, mb_per_sec};
}

}  // namespace

TEST(FileWriterPerfComparison, PositionalVsMutexFstream) {
  std::vector<PerfResult> results;
  results.push_back(RunChunkedWorkload("Positional"));
  results.push_back(RunChunkedWorkload("MutexFstream"));

  std::cout << "\n=== IFileWriter perf comparison ===\n";
  std::cout << "Workload: 32 workers, 8 chunks/worker, 2 MB chunks, 64 KB sink pieces (512 MB total)\n";
  for (const auto& r : results) {
    std::cout << "  " << r.kind << ": " << r.elapsed_ms << " ms ("
              << static_cast<int>(r.mb_per_sec) << " MB/s)\n";
  }
  std::cout << "===================================\n" << std::endl;

  // Sanity: both should make positive progress; perf is informational.
  for (const auto& r : results) {
    EXPECT_GT(r.mb_per_sec, 0.0) << r.kind;
  }
}
