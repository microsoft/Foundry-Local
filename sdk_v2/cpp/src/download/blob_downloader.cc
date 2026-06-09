// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "download/blob_downloader.h"
#include "download/blob_download_state.h"
#include "exception.h"
#include "logger.h"
#include "util/path_safety.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <future>
#include <mutex>
#include <string>
#include <vector>

#include <azure/core/context.hpp>
#include <azure/storage/blobs.hpp>

namespace fl {

// ========================================================================
// AzureBlobDownloader — real Azure Storage SDK implementation
// ========================================================================

/// Per-blob shared state passed to the protected virtuals. The production
/// virtuals dereference `blob_client` / `azure_ctx`; tests can ignore them.
/// `cancel_flag` is flipped by the orchestrator on the first chunk failure so
/// workers exit promptly without waiting for Azure SDK timeouts.
struct AzureBlobDownloader::ChunkContext {
  Azure::Storage::Blobs::BlobClient* blob_client;
  Azure::Core::Context* azure_ctx;
  std::atomic<bool>* cancel_flag;
};

AzureBlobDownloader::AzureBlobDownloader(ILogger* logger) : logger_(logger) {}

std::vector<BlobItemInfo> AzureBlobDownloader::ListBlobs(const std::string& sas_uri) {
  try {
    auto container_client = Azure::Storage::Blobs::BlobContainerClient(sas_uri);
    std::vector<BlobItemInfo> items;

    for (auto page = container_client.ListBlobs(); page.HasPage(); page.MoveToNextPage()) {
      for (const auto& blob : page.Blobs) {
        BlobItemInfo info;
        info.name = blob.Name;
        info.content_length = blob.BlobSize;
        items.push_back(std::move(info));
      }
    }

    return items;
  } catch (const Azure::Core::RequestFailedException& e) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
             std::string("failed to list blobs: ") + e.what());
  }
}

int64_t AzureBlobDownloader::GetBlobSize(ChunkContext& ctx) {
  auto props = ctx.blob_client->GetProperties({}, *ctx.azure_ctx).Value;
  return props.BlobSize;
}

std::atomic<bool>* AzureBlobDownloader::GetCancelFlag(ChunkContext& ctx) {
  return ctx.cancel_flag;
}

void AzureBlobDownloader::DownloadChunkToBuffer(ChunkContext& ctx,
                                                int64_t offset,
                                                int64_t size,
                                                std::vector<uint8_t>& buffer) {
  Azure::Storage::Blobs::DownloadBlobOptions range_opts;
  range_opts.Range = Azure::Core::Http::HttpRange{offset, size};
  auto result = ctx.blob_client->Download(range_opts, *ctx.azure_ctx);
  auto& body_stream = *result.Value.BodyStream;

  buffer.assign(static_cast<size_t>(size), 0);
  size_t total_read = 0;
  while (total_read < static_cast<size_t>(size)) {
    size_t bytes_read = body_stream.Read(buffer.data() + total_read,
                                         static_cast<size_t>(size) - total_read,
                                         *ctx.azure_ctx);
    if (bytes_read == 0) {
      break;
    }

    total_read += bytes_read;
  }

  // A zero-byte read before reaching `size` indicates the server closed early.
  // Treat as a hard error rather than silently writing a truncated chunk.
  if (total_read < static_cast<size_t>(size)) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
             "short read from blob stream: got " + std::to_string(total_read) + " of " +
                 std::to_string(size) + " bytes at offset " + std::to_string(offset));
  }
  buffer.resize(total_read);
}

namespace {

/// Open the local file at the given offset for write. Throws on failure.
void WriteChunkToFile(const std::string& local_path, int64_t offset,
                      const std::vector<uint8_t>& buffer, std::mutex& file_mutex) {
  std::lock_guard<std::mutex> lock(file_mutex);
  std::ofstream f(local_path, std::ios::binary | std::ios::in | std::ios::out);
  if (!f.is_open()) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
             "failed to open blob file for write: " + local_path);
  }

  f.seekp(offset);
  f.write(reinterpret_cast<const char*>(buffer.data()),
          static_cast<std::streamsize>(buffer.size()));
  if (f.fail()) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
             "failed to write blob chunk to " + local_path + " at offset " +
                 std::to_string(offset) + " (" + std::to_string(buffer.size()) + " bytes)");
  }
}

/// Pre-allocate `local_path` to `blob_size` bytes if it does not already exist
/// at the expected size. Allows concurrent chunk writes to seek without races
/// and avoids re-zeroing a file we're resuming.
void EnsureFilePreallocated(const std::string& local_path, int64_t blob_size) {
  std::error_code ec;
  auto cur_size = std::filesystem::file_size(local_path, ec);
  if (!ec && cur_size == static_cast<uintmax_t>(blob_size)) {
    return;
  }

  std::ofstream f(local_path, std::ios::binary);
  if (!f.is_open()) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
             "failed to open blob file for pre-allocation: " + local_path);
  }

  f.seekp(blob_size - 1);
  f.put('\0');
  f.close();
  if (f.fail()) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
             "failed to pre-allocate blob file: " + local_path +
                 " (size=" + std::to_string(blob_size) + ")");
  }
}

}  // namespace

void AzureBlobDownloader::DownloadBlob(const std::string& sas_uri,
                                       const std::string& blob_name,
                                       const std::string& local_path,
                                       int max_concurrency,
                                       BlobBytesWrittenFn bytes_written_cb,
                                       std::atomic<bool>* cancelled) {
  try {
    // Configure retry at the SDK level instead of a manual retry loop.
    // Exponential backoff: 2s initial delay, 30s cap, generous retry count.
    Azure::Storage::Blobs::BlobClientOptions client_options;
    client_options.Retry.MaxRetries = 10;
    client_options.Retry.RetryDelay = std::chrono::milliseconds{2000};
    client_options.Retry.MaxRetryDelay = std::chrono::milliseconds{30000};
    // Add 429 (TooManyRequests) to the default retryable status codes.
    // Defaults already include: 408, 500, 502, 503, 504.
    client_options.Retry.StatusCodes.insert(Azure::Core::Http::HttpStatusCode::TooManyRequests);

    auto container_client = Azure::Storage::Blobs::BlobContainerClient(sas_uri, client_options);
    auto blob_client = container_client.GetBlobClient(blob_name);

    // Single shared Azure context for the whole blob; calling Cancel() on it
    // propagates into every in-flight chunk read.
    Azure::Core::Context azure_ctx;
    // Internal cancel flag flipped by the orchestrator on first chunk failure
    // or by external cancellation; checked by workers between iterations.
    std::atomic<bool> internal_cancel{false};

    ChunkContext chunk_ctx{&blob_client, &azure_ctx, &internal_cancel};

    int64_t blob_size = GetBlobSize(chunk_ctx);

    if (blob_size == 0) {
      // Empty blob — just create the file and clean up any stale sidecar.
      std::ofstream f(local_path, std::ios::binary);
      if (!f.is_open()) {
        FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
                 "failed to create empty blob file: " + local_path);
      }
      f.close();
      BlobDownloadState::DeleteState(local_path, logger_);
      return;
    }

    // 2MB chunk size matching C#
    constexpr int64_t kChunkSize = 2 * 1024 * 1024;
    int32_t num_chunks = static_cast<int32_t>((blob_size + kChunkSize - 1) / kChunkSize);

    // Resume from existing sidecar if it matches the current blob layout.
    auto state = BlobDownloadState::LoadState(blob_name, local_path, blob_size,
                                              static_cast<int32_t>(kChunkSize),
                                              num_chunks, logger_);
    if (!state) {
      state = BlobDownloadState::CreateNew(blob_name, local_path, blob_size,
                                           static_cast<int32_t>(kChunkSize), num_chunks);
    }

    // Pre-allocate only if the file is not already at full size. On resume the
    // file already exists with valid bytes in completed chunks; re-truncating
    // would discard them.
    EnsureFilePreallocated(local_path, blob_size);

    // Track cumulative bytes for progress reporting; seed with bytes already
    // present on disk so percent stays monotonic across resume.
    std::atomic<int64_t> bytes_completed{state->CalculateDownloadedSize()};
    if (bytes_written_cb && bytes_completed.load() > 0) {
      bytes_written_cb(bytes_completed.load());
    }

    auto pending = state->GetPendingChunks();
    if (pending.empty()) {
      // Already complete on disk — drop the sidecar.
      BlobDownloadState::DeleteState(local_path, logger_);
      if (bytes_written_cb) {
        bytes_written_cb(blob_size);
      }
      return;
    }

    // Save the sidecar roughly every 2% of chunks, with a floor of 10.
    const int32_t save_interval = std::max(10, num_chunks / 50);
    std::atomic<int32_t> chunks_since_save{0};

    // Mutex protects concurrent writes to different offsets in the same file.
    std::mutex file_mutex;
    std::mutex error_mutex;
    std::exception_ptr first_error;

    // Worker pool: workers race to claim from `pending` via atomic fetch_add.
    // On any failure, the first worker to fail records the error, sets
    // internal_cancel, and calls azure_ctx.Cancel(); other workers see the
    // signal and exit fast.
    std::atomic<size_t> next_pending_idx{0};
    int worker_count = std::min<int>(max_concurrency, static_cast<int>(pending.size()));
    if (worker_count < 1) {
      worker_count = 1;
    }
    std::vector<std::future<void>> workers;
    workers.reserve(static_cast<size_t>(worker_count));

    auto worker_body = [&]() {
      while (true) {
        // External cancellation drains the pool as fast as the SDK can unwind.
        if (cancelled && cancelled->load(std::memory_order_relaxed)) {
          if (!internal_cancel.exchange(true)) {
            azure_ctx.Cancel();
          }
          return;
        }
        if (internal_cancel.load(std::memory_order_relaxed)) {
          return;
        }

        size_t i = next_pending_idx.fetch_add(1, std::memory_order_relaxed);
        if (i >= pending.size()) {
          return;
        }
        int32_t chunk_idx = pending[i];
        int64_t offset = static_cast<int64_t>(chunk_idx) * kChunkSize;
        int64_t size = std::min<int64_t>(kChunkSize, blob_size - offset);

        std::vector<uint8_t> buffer;
        try {
          DownloadChunkToBuffer(chunk_ctx, offset, size, buffer);
        } catch (...) {
          std::lock_guard<std::mutex> lock(error_mutex);
          if (!first_error) {
            first_error = std::current_exception();
          }
          if (!internal_cancel.exchange(true)) {
            azure_ctx.Cancel();
          }
          return;
        }

        try {
          WriteChunkToFile(local_path, offset, buffer, file_mutex);
        } catch (...) {
          std::lock_guard<std::mutex> lock(error_mutex);
          if (!first_error) {
            first_error = std::current_exception();
          }
          if (!internal_cancel.exchange(true)) {
            azure_ctx.Cancel();
          }
          return;
        }

        int64_t new_total = bytes_completed.fetch_add(size, std::memory_order_relaxed) + size;
        if (bytes_written_cb) {
          bytes_written_cb(new_total);
        }

        bool should_save = false;
        {
          std::lock_guard<std::mutex> lock(state->mutex());
          state->MarkChunkComplete(chunk_idx);
          int32_t inc = chunks_since_save.fetch_add(1, std::memory_order_relaxed) + 1;
          if (inc >= save_interval) {
            chunks_since_save.store(0, std::memory_order_relaxed);
            should_save = true;
          }
        }
        if (should_save) {
          std::lock_guard<std::mutex> lock(state->mutex());
          state->SaveState(logger_);
        }
      }
    };

    for (int w = 0; w < worker_count; ++w) {
      workers.push_back(std::async(std::launch::async, worker_body));
    }

    for (auto& f : workers) {
      try {
        f.get();
      } catch (...) {
        // Worker bodies should already have routed exceptions through
        // first_error, but stay defensive in case std::async signals one.
        std::lock_guard<std::mutex> lock(error_mutex);
        if (!first_error) {
          first_error = std::current_exception();
        }
        internal_cancel.store(true, std::memory_order_relaxed);
      }
    }

    if (first_error || (cancelled && cancelled->load(std::memory_order_relaxed))) {
      // Persist what we have so the next attempt resumes from here.
      {
        std::lock_guard<std::mutex> lock(state->mutex());
        state->SaveState(logger_);
      }
      if (cancelled && cancelled->load(std::memory_order_relaxed)) {
        FL_THROW(FOUNDRY_LOCAL_ERROR_OPERATION_CANCELLED, "download cancelled");
      }
      std::rethrow_exception(first_error);
    }

    // All chunks done — sidecar is no longer needed.
    BlobDownloadState::DeleteState(local_path, logger_);
  } catch (const Azure::Core::OperationCancelledException&) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_OPERATION_CANCELLED, "download cancelled");
  } catch (const Azure::Core::RequestFailedException& e) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
             std::string("failed to download blob '") + blob_name + "': " + e.what());
  }
}

// ========================================================================
// DownloadBlobsToDirectory — high-level orchestration
// ========================================================================

namespace {

/// Compute relative destination path by stripping the prefix from blob name.
/// Matches C# GetItemDestinationPath_AzureFoundryImpl.
std::string ComputeRelativePath(const std::string& prefix, const std::string& blob_name) {
  if (prefix.empty()) {
    return blob_name;
  }

  if (blob_name.size() <= prefix.size()) {
    return blob_name;
  }

  auto trim = prefix.size();
  if (blob_name[trim] == '/') {
    ++trim;
  }

  return blob_name.substr(trim);
}

bool EndsWith(const std::string& str, const std::string& suffix) {
  if (suffix.size() > str.size()) {
    return false;
  }

  return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin(),
                    [](char a, char b) {
                      return std::tolower(static_cast<unsigned char>(a)) ==
                             std::tolower(static_cast<unsigned char>(b));
                    });
}

/// Returns false if a file at `local_path` already matches the blob's expected
/// `content_length` exactly AND has no `.dlstate` sidecar — in which case the
/// caller can skip the download. Returns true (download needed) for any of:
/// missing file, size mismatch, sidecar present (file may be pre-allocated
/// with holes), or filesystem-stat errors (treat as "redownload to be safe").
bool IsDownloadNeeded(const BlobItemInfo& blob, const std::string& local_path) {
  std::error_code ec;
  auto status = std::filesystem::status(local_path, ec);
  if (ec || !std::filesystem::exists(status) || !std::filesystem::is_regular_file(status)) {
    return true;
  }
  auto size = std::filesystem::file_size(local_path, ec);
  if (ec) {
    return true;
  }
  if (static_cast<int64_t>(size) != blob.content_length) {
    return true;
  }
  // The data file is at the expected size, but a sidecar means a previous run
  // pre-allocated then aborted mid-download. The file has holes; let
  // AzureBlobDownloader resume from the sidecar.
  auto sidecar = BlobDownloadState::GetStateFilePath(local_path);
  if (std::filesystem::exists(sidecar, ec)) {
    return true;
  }
  return false;
}

}  // anonymous namespace

void DownloadBlobsToDirectory(IBlobDownloader& downloader,
                              const std::string& sas_uri,
                              const std::string& output_directory,
                              const BlobDownloadOptions& options) {
  // Step 1: Enumerate all blobs
  auto all_blobs = downloader.ListBlobs(sas_uri);

  // Step 2: Filter by path prefix
  std::vector<std::pair<BlobItemInfo, std::string>> blobs_to_download;

  for (auto& blob : all_blobs) {
    if (!options.path_prefix.empty() &&
        blob.name.compare(0, options.path_prefix.size(), options.path_prefix) != 0) {
      continue;
    }

    auto relative_path = ComputeRelativePath(options.path_prefix, blob.name);

    // Normalize backslashes to forward slashes so path-traversal validation
    // is cross-platform: on POSIX, backslash is just a filename character and
    // would otherwise hide a `..\evil` segment from the canonicalization check.
    std::replace(relative_path.begin(), relative_path.end(), '\\', '/');

    auto local_path_fs = std::filesystem::path(output_directory) / relative_path;

    // Defense-in-depth: blob names come from a remote service. Reject any
    // blob whose computed destination would escape `output_directory` via
    // `..` segments or an absolute path. This check must happen BEFORE any
    // file create/open/write so a malicious entry cannot touch the disk.
    if (!IsPathWithinDirectory(local_path_fs, std::filesystem::path(output_directory))) {
      FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT,
               std::string("blob path escapes destination directory: ") + blob.name);
    }

    auto local_path = local_path_fs.string();
    blobs_to_download.emplace_back(std::move(blob), std::move(local_path));
  }

  // Step 3: Filter out inference_model.json (matching C# AzureBlobDownloadClientProvider filter)
  blobs_to_download.erase(std::remove_if(blobs_to_download.begin(), blobs_to_download.end(),
                                         [](const auto& pair) {
                                           return EndsWith(pair.first.name, "inference_model.json");
                                         }),
                          blobs_to_download.end());

  if (blobs_to_download.empty()) {
    return;
  }

  // Step 3.5: Sort smallest-first for faster initial perceived progress
  std::sort(blobs_to_download.begin(), blobs_to_download.end(),
            [](const auto& a, const auto& b) {
              return a.first.content_length < b.first.content_length;
            });

  // Step 4: Calculate total size across every in-scope blob, including those
  // already present on disk — so 100% always means "every byte is local".
  int64_t total_size = 0;
  for (const auto& [blob, _] : blobs_to_download) {
    total_size += blob.content_length;
  }

  // Step 4.25: Skip blobs already present at the expected size. Their bytes
  // count toward "downloaded" so the percentage stays accurate when this is a
  // resume of a partially-completed download.
  int64_t skipped_bytes = 0;
  blobs_to_download.erase(
      std::remove_if(blobs_to_download.begin(), blobs_to_download.end(),
                     [&skipped_bytes](const auto& pair) {
                       if (IsDownloadNeeded(pair.first, pair.second)) {
                         return false;
                       }
                       skipped_bytes += pair.first.content_length;
                       return true;
                     }),
      blobs_to_download.end());

  // Step 4.5: Emit initial progress reflecting any already-on-disk bytes.
  // If everything was skipped, emit 100% directly and return.
  if (blobs_to_download.empty()) {
    if (options.progress) {
      options.progress(100.0f);
    }
    return;
  }

  if (options.progress) {
    float initial_percent = total_size > 0
                                ? static_cast<float>(skipped_bytes) /
                                      static_cast<float>(total_size) * 100.0f
                                : 0.0f;
    int result = options.progress(initial_percent);
    if (result != 0) {
      FL_THROW(FOUNDRY_LOCAL_ERROR_OPERATION_CANCELLED, "download cancelled by user callback return value");
    }
  }

  // Step 5: Download each blob with per-chunk progress.
  // The cancellation flag is set when the progress callback returns non-zero.
  // It is shared with chunk download threads so they can exit promptly.
  std::atomic<bool> cancelled{false};
  // Seed with skipped bytes so per-chunk progress callbacks compute the right
  // overall percentage.
  std::atomic<int64_t> total_downloaded_bytes{skipped_bytes};

  for (const auto& [blob, local_path] : blobs_to_download) {
    // Check cancellation between blobs
    if (cancelled.load(std::memory_order_relaxed)) {
      FL_THROW(FOUNDRY_LOCAL_ERROR_OPERATION_CANCELLED, "download cancelled by user callback return value");
    }

    // Ensure parent directory exists
    auto parent = std::filesystem::path(local_path).parent_path();
    if (!parent.empty()) {
      std::filesystem::create_directories(parent);
    }

    // Capture the byte count at the start of this blob so per-chunk callbacks
    // can compute total progress across all blobs.
    int64_t blob_base_bytes = total_downloaded_bytes.load();

    BlobBytesWrittenFn per_chunk_progress;
    if (options.progress && total_size > 0) {
      per_chunk_progress = [&](int64_t blob_bytes_so_far) {
        int64_t overall = blob_base_bytes + blob_bytes_so_far;
        // Don't exceed total_size (may happen if blob sizes changed between list and download)
        overall = std::min(overall, total_size);

        float percent = static_cast<float>(overall) / static_cast<float>(total_size) * 100.0f;
        int result = options.progress(percent);
        if (result != 0) {
          cancelled.store(true, std::memory_order_relaxed);
          FL_THROW(FOUNDRY_LOCAL_ERROR_OPERATION_CANCELLED, "download cancelled by user callback return value");
        }
      };
    }

    downloader.DownloadBlob(sas_uri, blob.name, local_path, options.max_concurrency,
                            per_chunk_progress, &cancelled);

    total_downloaded_bytes += blob.content_length;
  }

  // Final progress
  if (options.progress) {
    options.progress(100.0f);
  }
}

}  // namespace fl
