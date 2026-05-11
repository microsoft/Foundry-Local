// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "download/download_manager.h"
#include "download/inference_model_writer.h"
#include "exception.h"

#include <foundry_local/foundry_local_c.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace fl {

namespace {

const char* kDownloadSignalFileName = "download.tmp";
const char* kGenAIConfigFileName = "genai_config.json";
const char* kInferenceModelFileName = "inference_model.json";

/// Check whether inference_model.json exists at the root or in any immediate
/// subdirectory.  This is the definitive proof that a download completed
/// successfully — DownloadModel writes it in Step 3.
bool HasInferenceModelJson(const std::string& model_path) {
  if (std::filesystem::exists(std::filesystem::path(model_path) / kInferenceModelFileName)) {
    return true;
  }

  for (const auto& entry : std::filesystem::directory_iterator(model_path)) {
    if (entry.is_directory()) {
      if (std::filesystem::exists(entry.path() / kInferenceModelFileName)) {
        return true;
      }
    }
  }

  return false;
}

/// Resolve the effective model path — the directory containing genai_config.json.
/// For single-variant models this is model_path itself.
/// For multi-variant models it's the first subdirectory containing genai_config.json.
std::string ResolveEffectiveModelPath(const std::string& model_path) {
  auto root_config = std::filesystem::path(model_path) / kGenAIConfigFileName;
  if (std::filesystem::exists(root_config)) {
    return model_path;
  }

  for (const auto& entry : std::filesystem::directory_iterator(model_path)) {
    if (entry.is_directory()) {
      auto sub_config = entry.path() / kGenAIConfigFileName;
      if (std::filesystem::exists(sub_config)) {
        return entry.path().string();
      }
    }
  }

  // No genai_config.json found — return root
  return model_path;
}

/// Convert ':<version>' suffix to '-<version>' for filesystem compatibility.
/// Matches C# AzureFoundryLocalDownloadClientProvider.GetModelDirectoryName.
std::string FixVersionSuffix(const std::string& name) {
  auto last_colon = name.rfind(':');
  if (last_colon == std::string::npos || last_colon == 0) {
    return name;
  }

  // Check that what follows the colon is a number
  auto suffix = name.substr(last_colon + 1);
  bool all_digits = !suffix.empty();
  for (char c : suffix) {
    if (c < '0' || c > '9') {
      all_digits = false;
      break;
    }
  }

  if (!all_digits) {
    return name;
  }

  return name.substr(0, last_colon) + "-" + suffix;
}

}  // anonymous namespace

DownloadManager::DownloadManager(std::string cache_directory, std::string catalog_region, int max_concurrency)
    : cache_directory_(std::move(cache_directory)),
      max_concurrency_(max_concurrency),
      registry_client_(std::make_unique<ModelRegistryClient>(std::move(catalog_region))),
      blob_downloader_(std::make_unique<AzureBlobDownloader>()) {}

DownloadManager::~DownloadManager() = default;

void DownloadManager::SetModelRegistryClient(std::unique_ptr<ModelRegistryClient> client) {
  registry_client_ = std::move(client);
}

void DownloadManager::SetBlobDownloader(std::unique_ptr<IBlobDownloader> downloader) {
  blob_downloader_ = std::move(downloader);
}

std::string DownloadManager::ComputeModelPath(const ModelInfo& info) const {
  // Get publisher from string properties
  std::string publisher;
  auto it = info.string_properties.find(FOUNDRY_LOCAL_MODEL_PROP_PUBLISHER_STR);
  if (it != info.string_properties.end()) {
    publisher = it->second;
  }

  // model_id format is "name:version" — fix for filesystem
  std::string model_dir_name = FixVersionSuffix(info.model_id);

  if (publisher.empty()) {
    return (std::filesystem::path(cache_directory_) / model_dir_name).string();
  }

  return (std::filesystem::path(cache_directory_) / publisher / model_dir_name).string();
}

std::string DownloadManager::DownloadModel(const ModelInfo& info,
                                           std::function<void(float)> progress_cb) {
  auto model_path = ComputeModelPath(info);

  // Check if already downloaded (before validating URI — cached models don't need one).
  // A valid cache hit requires: directory exists, no in-progress signal file, and
  // inference_model.json is present (written by DownloadModel on successful completion).
  auto signal_path = std::filesystem::path(model_path) / kDownloadSignalFileName;
  if (std::filesystem::exists(model_path) && !std::filesystem::exists(signal_path) &&
      HasInferenceModelJson(model_path)) {
    // Already cached and download was complete
    if (progress_cb) {
      progress_cb(100.0f);
    }

    return ResolveEffectiveModelPath(model_path);
  }

  if (info.uri.empty()) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL, "cannot download model: empty URI (asset_id)");
  }

  // Create output directory
  std::filesystem::create_directories(model_path);

  // Create download signal file
  {
    std::ofstream signal(signal_path);
    // Empty file — its presence indicates download is in progress
  }

  // Emit 0% immediately so callers know the download process has started.
  // This provides a heartbeat during the silent container resolution phase.
  if (progress_cb) {
    progress_cb(0.0f);
  }

  try {
    // Step 1: Resolve SAS URI
    auto container = registry_client_->ResolveModelContainer(info.uri);

    if (container.blob_sas_uri.empty()) {
      FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL, "model registry returned empty SAS URI for: " + info.uri);
    }

    // Step 2: Download blobs
    BlobDownloadOptions download_opts;
    // The ModelInfo doesn't currently carry a path prefix, but the URI is the asset_id
    // and the blob container has all files at the root or in variant subdirectories.
    download_opts.path_prefix = "";
    download_opts.max_concurrency = max_concurrency_;

    if (progress_cb) {
      download_opts.progress = [&progress_cb](float percent) {
        progress_cb(percent);
        return 0;
      };
    }

    DownloadBlobsToDirectory(*blob_downloader_, container.blob_sas_uri,
                             model_path, download_opts);

    // Step 3: Write inference_model.json — use model_id (includes version) so the
    // local model scanner can match it back to catalog entries during startup.
    WriteInferenceModelJson(model_path, info.model_id, info.prompt_templates);

    // Step 4: Fix variant download
    FixVariantInferenceModelJson(model_path);

    // Step 5: Remove download signal — marks download as complete
    std::filesystem::remove(signal_path);

    return ResolveEffectiveModelPath(model_path);
  } catch (...) {
    // Leave the signal file in place so the incomplete download is detected
    throw;
  }
}

bool DownloadManager::IsModelCached(const ModelInfo& info) const {
  auto model_path = ComputeModelPath(info);
  if (!std::filesystem::exists(model_path)) {
    return false;
  }

  // A valid cache requires no in-progress signal file AND inference_model.json
  // (written on successful download completion). An empty directory or one
  // without inference_model.json is not a valid cache hit.
  auto signal_path = std::filesystem::path(model_path) / kDownloadSignalFileName;
  return !std::filesystem::exists(signal_path) && HasInferenceModelJson(model_path);
}

std::string DownloadManager::GetModelCachePath(const ModelInfo& info) const {
  auto model_path = ComputeModelPath(info);
  if (std::filesystem::exists(model_path)) {
    return model_path;
  }

  return {};
}

}  // namespace fl
