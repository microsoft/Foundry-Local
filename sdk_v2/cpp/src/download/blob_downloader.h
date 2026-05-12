// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace fl {

/// Returns true if `candidate` resolves to a path inside (or equal to) `root`.
/// Uses std::filesystem::weakly_canonical so the paths need not exist on disk.
/// This is used to defend against path-traversal where a blob name contains
/// `..` segments or absolute paths that would escape the destination directory.
bool IsPathWithinDirectory(const std::filesystem::path& candidate,
                           const std::filesystem::path& root);


/// Progress callback: percent is 0.0 to 100.0. Return 0 to continue, non-zero to cancel.
using DownloadProgressFn = std::function<int(float percent)>;

/// Per-chunk progress callback used during a single blob download.
/// Receives the number of bytes written so far for that blob.
using BlobBytesWrittenFn = std::function<void(int64_t bytes_written)>;

/// Options for blob download operations.
struct BlobDownloadOptions {
  /// Path prefix filter — only download blobs whose name starts with this.
  std::string path_prefix;

  /// Maximum concurrent chunk downloads per blob. Default matches C# desktop.
  int max_concurrency = 64;

  /// Progress callback (optional). Return non-zero to cancel the download.
  DownloadProgressFn progress;
};

/// Information about a single blob in a container.
struct BlobItemInfo {
  std::string name;
  int64_t content_length = 0;
};

/// Interface for blob download operations. Enables testing via mock implementations.
class IBlobDownloader {
 public:
  virtual ~IBlobDownloader() = default;

  /// List all blobs in the container at the given SAS URI.
  virtual std::vector<BlobItemInfo> ListBlobs(const std::string& sas_uri) = 0;

  /// Download a single blob to a local file path.
  /// The sas_uri is the container SAS URI; blob_name identifies the blob within it.
  /// If bytes_written_cb is provided, it is called after each chunk with the cumulative byte count.
  /// If cancelled is non-null, the download checks the flag and aborts promptly when set.
  virtual void DownloadBlob(const std::string& sas_uri,
                            const std::string& blob_name,
                            const std::string& local_path,
                            int max_concurrency,
                            BlobBytesWrittenFn bytes_written_cb = nullptr,
                            std::atomic<bool>* cancelled = nullptr) = 0;
};

/// Azure Storage Blobs SDK-based implementation of IBlobDownloader.
class AzureBlobDownloader : public IBlobDownloader {
 public:
  std::vector<BlobItemInfo> ListBlobs(const std::string& sas_uri) override;

  void DownloadBlob(const std::string& sas_uri,
                    const std::string& blob_name,
                    const std::string& local_path,
                    int max_concurrency,
                    BlobBytesWrittenFn bytes_written_cb = nullptr,
                    std::atomic<bool>* cancelled = nullptr) override;
};

/// High-level download function: enumerate, filter, and download all blobs from a SAS URI.
/// Handles safetensors optimization, path prefix filtering, and progress reporting.
/// Throws fl::Exception on failure.
void DownloadBlobsToDirectory(IBlobDownloader& downloader,
                              const std::string& sas_uri,
                              const std::string& output_directory,
                              const BlobDownloadOptions& options);

}  // namespace fl
