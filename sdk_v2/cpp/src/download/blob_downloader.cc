// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "download/blob_downloader.h"
#include "exception.h"
#include "util/path_safety.h"

#include <algorithm>
#include <atomic>
#include <chrono>
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

    // Context provides cooperative cancellation across all SDK operations.
    Azure::Core::Context ctx;

    // Get blob size
    auto props = blob_client.GetProperties({}, ctx).Value;
    int64_t blob_size = props.BlobSize;

    if (blob_size == 0) {
      // Empty blob — just create the file
      std::ofstream f(local_path, std::ios::binary);
      if (!f.is_open()) {
        FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
                 "failed to create empty blob file: " + local_path);
      }

      return;
    }

    // 2MB chunk size matching C#
    constexpr int64_t kChunkSize = 2 * 1024 * 1024;
    int64_t num_chunks = (blob_size + kChunkSize - 1) / kChunkSize;

    // Pre-allocate the file to the full blob size.
    // This lets concurrent chunk writes seek to their offset without a resize race.
    {
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

    // Track cumulative bytes for progress reporting
    std::atomic<int64_t> bytes_completed{0};

    // Mutex protects concurrent writes to different offsets in the same file.
    // Each chunk opens the file, seeks, and writes — the mutex prevents interleaved I/O.
    std::mutex file_mutex;

    // Download chunks concurrently using a bounded pool of async tasks.
    // We launch up to max_concurrency tasks at a time, then wait for the batch to complete.
    for (int64_t batch_start = 0; batch_start < num_chunks; batch_start += max_concurrency) {
      // Check cancellation between batches
      if (cancelled && cancelled->load(std::memory_order_relaxed)) {
        ctx.Cancel();
        FL_THROW(FOUNDRY_LOCAL_ERROR_OPERATION_CANCELLED, "download cancelled");
      }

      int64_t batch_end = std::min(batch_start + max_concurrency, num_chunks);
      std::vector<std::future<void>> futures;
      futures.reserve(static_cast<size_t>(batch_end - batch_start));

      for (int64_t chunk_idx = batch_start; chunk_idx < batch_end; ++chunk_idx) {
        int64_t offset = chunk_idx * kChunkSize;
        int64_t size = std::min(kChunkSize, blob_size - offset);

        futures.push_back(std::async(std::launch::async,
                                     [&blob_client, &local_path, &file_mutex, &bytes_completed, &bytes_written_cb,
                                      &ctx, offset, size]() {
                                       // Download this range from the blob.
                                       // Retry and backoff are handled by the SDK's retry policy.
                                       Azure::Storage::Blobs::DownloadBlobOptions range_opts;
                                       range_opts.Range = Azure::Core::Http::HttpRange{offset, size};
                                       auto result = blob_client.Download(range_opts, ctx);
                                       auto& body_stream = *result.Value.BodyStream;

                                       // Read the body into a local buffer
                                       std::vector<uint8_t> buffer(static_cast<size_t>(size));
                                       size_t total_read = 0;
                                       while (total_read < static_cast<size_t>(size)) {
                                         size_t bytes_read = body_stream.Read(
                                             buffer.data() + total_read,
                                             static_cast<size_t>(size) - total_read,
                                             ctx);

                                         if (bytes_read == 0) {
                                           break;
                                         }

                                         total_read += bytes_read;
                                       }

                                       // a zero-byte read before reaching `size` indicates the server closed early.
                                       // Treat as a hard error rather than silently writing a truncated chunk.
                                       if (total_read < static_cast<size_t>(size)) {
                                         FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
                                                  "short read from blob stream: got " +
                                                      std::to_string(total_read) + " of " +
                                                      std::to_string(size) + " bytes at offset " +
                                                      std::to_string(offset));
                                       }

                                       // Write the chunk to the file at the correct offset
                                       {
                                         std::lock_guard<std::mutex> lock(file_mutex);
                                         std::ofstream f(local_path,
                                                         std::ios::binary | std::ios::in | std::ios::out);
                                         if (!f.is_open()) {
                                           FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
                                                    "failed to open blob file for write: " + local_path);
                                         }

                                         f.seekp(offset);
                                         f.write(reinterpret_cast<const char*>(buffer.data()),
                                                 static_cast<std::streamsize>(total_read));
                                         if (f.fail()) {
                                           FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,
                                                    "failed to write blob chunk to " + local_path +
                                                        " at offset " + std::to_string(offset) +
                                                        " (" + std::to_string(total_read) + " bytes)");
                                         }
                                       }

                                       // Report progress
                                       bytes_completed += static_cast<int64_t>(total_read);
                                       if (bytes_written_cb) {
                                         bytes_written_cb(bytes_completed.load());
                                       }
                                     }));
      }

      // Wait for all tasks in this batch, cancelling context on failure
      try {
        for (auto& f : futures) {
          f.get();
        }
      } catch (...) {
        // Cancel remaining in-flight downloads so futures complete quickly
        ctx.Cancel();
        for (auto& f : futures) {
          try {
            if (f.valid()) {
              f.get();
            }
          } catch (...) {
          }
        }
        throw;
      }
    }
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

  // Step 4: Calculate total size for progress
  int64_t total_size = 0;
  for (const auto& [blob, _] : blobs_to_download) {
    total_size += blob.content_length;
  }

  // Step 4.5: Emit 0% so callers know the download has started
  if (options.progress) {
    int result = options.progress(0.0f);
    if (result != 0) {
      FL_THROW(FOUNDRY_LOCAL_ERROR_OPERATION_CANCELLED, "download cancelled by user callback return value");
    }
  }

  // Step 5: Download each blob with per-chunk progress.
  // The cancellation flag is set when the progress callback returns non-zero.
  // It is shared with chunk download threads so they can exit promptly.
  std::atomic<bool> cancelled{false};
  std::atomic<int64_t> total_downloaded_bytes{0};

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
