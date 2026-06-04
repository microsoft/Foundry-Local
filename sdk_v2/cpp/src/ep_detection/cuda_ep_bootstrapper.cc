// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "ep_detection/cuda_ep_bootstrapper.h"

#include "http/http_client.h"
#include "http/http_download.h"
#include "logger.h"
#include "util/file_lock.h"
#include "util/sha256.h"
#include "util/zip_extract.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <string>
#include <unordered_map>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {

constexpr const char* kPackageFileName = "cuda-ep.zip";
constexpr const char* kLockFileName = "cuda-ep.lock";
constexpr const char* kUserAgent = "FoundryLocal";
constexpr int kMaxInstallAttempts = 5;

// Manifest URL on the CDN — published by the CUDA EP upload pipeline.
constexpr const char* kManifestUrl =
    "https://foundrypackages-ffhrdhbxb7gpdreh.b02.azurefd.net/cuda_ep_prod.json";

// Platform key used to look up the current platform in the manifest.
#if defined(_WIN32)
constexpr const char* kPlatformKey = "win-x64";
constexpr const char* kCudaProviderLib = "onnxruntime_providers_cuda.dll";
#else
constexpr const char* kPlatformKey = "linux-x64";
constexpr const char* kCudaProviderLib = "libonnxruntime_providers_cuda.so";
#endif

constexpr const char* kRegistrationName = "Foundry.CUDA";

struct ManifestInfo {
  std::string version;
  std::string download_url;
  std::unordered_map<std::string, std::string> sha256;  // filename -> expected hash
};

/// Fetch and parse the CUDA EP manifest from the CDN.
ManifestInfo FetchManifest(fl::ILogger& logger) {
  logger.Log(fl::LogLevel::Information,
             fmt::format("CUDA EP: fetching manifest from {}", kManifestUrl));

  auto body = fl::http::HttpGetWithRetry(kManifestUrl, kUserAgent, logger);
  auto j = nlohmann::json::parse(body);

  ManifestInfo info;
  info.version = j.at("version").get<std::string>();

  auto& packages = j.at("packages");
  if (!packages.contains(kPlatformKey)) {
    throw std::runtime_error(
        fmt::format("CUDA EP manifest has no entry for platform '{}'", kPlatformKey));
  }

  auto& pkg = packages.at(kPlatformKey);
  info.download_url = pkg.at("url").get<std::string>();

  for (auto& [filename, hash] : pkg.at("sha256").items()) {
    info.sha256[filename] = hash.get<std::string>();
  }

  return info;
}

/// Verify all expected binaries exist and have correct SHA256 hashes.
bool VerifyPackage(const std::filesystem::path& dir,
                   const std::unordered_map<std::string, std::string>& expected_hashes,
                   fl::ILogger& logger) {
  for (const auto& [filename, expected_hash] : expected_hashes) {
    auto file_path = dir / filename;

    if (!std::filesystem::exists(file_path)) {
      return false;
    }

    auto hash = fl::Sha256File(file_path);

    // Case-insensitive comparison
    if (!std::equal(hash.begin(), hash.end(), expected_hash.begin(), expected_hash.end(),
                    [](char a, char b) { return std::toupper(a) == std::toupper(b); })) {
      logger.Log(fl::LogLevel::Warning,
                 fmt::format("CUDA EP: hash mismatch for {}: got {}, expected {}",
                             filename, hash, expected_hash));
      return false;
    }
  }

  return true;
}

}  // anonymous namespace

namespace fl {

CudaEpBootstrapper::CudaEpBootstrapper(std::string ep_dir, EpRegistrationCallback register_ep)
    : ep_dir_(std::move(ep_dir)), register_ep_(std::move(register_ep)) {}

const std::string& CudaEpBootstrapper::Name() const {
  return name_;
}

bool CudaEpBootstrapper::IsRegistered() const {
  return registered_;
}

bool CudaEpBootstrapper::DownloadAndRegister(bool force,
                                             const ProgressCallback& progress_cb,
                                             ILogger& logger) {
  if (registered_ && !force) {
    if (progress_cb) {
      progress_cb(name_, 100.0f);
    }
    return true;
  }

  if (!force && attempts_ >= kMaxInstallAttempts) {
    logger.Log(LogLevel::Warning, "CUDA EP: max install attempts reached");
    return false;
  }

  attempts_++;

  auto ep_dir = std::filesystem::path(ep_dir_);
  auto lock_path = ep_dir.parent_path() / kLockFileName;
  auto zip_path = ep_dir.parent_path() / kPackageFileName;

  try {
    // Fetch the manifest to get the download URL and expected hashes
    auto manifest = FetchManifest(logger);
    logger.Log(LogLevel::Information,
               fmt::format("CUDA EP: manifest version={}, url={}", manifest.version, manifest.download_url));

    // Cross-process lock to prevent concurrent installs
    FileLock lock(lock_path);

    // Check if package already exists and is valid
    if (VerifyPackage(ep_dir, manifest.sha256, logger)) {
      logger.Log(LogLevel::Information, "CUDA EP: package already valid, skipping download");
    } else {
      // Clean up any partial install
      if (std::filesystem::exists(ep_dir)) {
        std::filesystem::remove_all(ep_dir);
      }

      std::filesystem::create_directories(ep_dir);

      // Download
      logger.Log(LogLevel::Information,
                 fmt::format("CUDA EP: downloading from {}", manifest.download_url));

      // Bridge callback-based cancellation to the atomic flag HttpDownloadFile expects
      std::atomic<bool> cancel_flag{false};

      auto download_progress = [&](float pct) {
        if (progress_cb) {
          // 0-80% for download phase
          if (!progress_cb(name_, pct * 0.8f)) {
            cancel_flag.store(true);
          }
        }
      };

      if (!HttpDownloadFile(manifest.download_url, zip_path, kUserAgent,
                            &cancel_flag, download_progress, logger)) {
        logger.Log(LogLevel::Warning, "CUDA EP: download failed (see prior log for details)");
        return false;
      }

      // Extract
      logger.Log(LogLevel::Information, "CUDA EP: extracting...");

      if (!ExtractZip(zip_path, ep_dir, logger)) {
        logger.Log(LogLevel::Warning, "CUDA EP: extraction failed");
        return false;
      }

      // Clean up zip
      std::filesystem::remove(zip_path);

      // Verify
      if (!VerifyPackage(ep_dir, manifest.sha256, logger)) {
        logger.Log(LogLevel::Warning, "CUDA EP: verification failed after download");
        return false;
      }
    }

    if (progress_cb) {
      progress_cb(name_, 90.0f);
    }

    // Register with ORT
#ifdef _WIN32
    // Permanently prepend the EP directory to PATH. The zip bundles all
    // required CUDA/cuDNN DLLs, so no system CUDA install is needed.
    // PATH must stay modified for the process lifetime because:
    //   - onnxruntime_providers_cuda.dll delay-loads some dependencies
    //   - onnxruntime-genai-cuda.dll is loaded later at model-load time
    //   - ORT creates CUDA sessions after registration
    {
      DWORD len = GetEnvironmentVariableW(L"PATH", nullptr, 0);
      std::wstring prev_path;
      if (len > 0) {
        prev_path.resize(len);
        GetEnvironmentVariableW(L"PATH", prev_path.data(), len);
        prev_path.resize(len - 1);  // remove trailing null
      }

      std::wstring new_path = ep_dir.wstring() + L";" + prev_path;
      SetEnvironmentVariableW(L"PATH", new_path.c_str());
    }
#endif

    auto cuda_lib_path = ep_dir / kCudaProviderLib;

    if (!register_ep_(kRegistrationName, cuda_lib_path)) {
      logger.Log(LogLevel::Warning, "CUDA EP: ORT registration failed");
      return false;
    }

    registered_ = true;

    if (progress_cb) {
      progress_cb(name_, 100.0f);
    }

    // Bootstrapper-side log — captures the install dir, which the central
    // register_ep callback (logs library + version) doesn't have.
    logger.Log(LogLevel::Information,
               fmt::format("CUDA EP: ready (install_path={})", ep_dir.string()));
    return true;
  } catch (const std::exception& e) {
    logger.Log(LogLevel::Warning, fmt::format("CUDA EP: error: {}", e.what()));
    return false;
  }
}

bool CudaEpBootstrapper::HasNvidiaGpu() {
#ifdef _WIN32
  FILE* pipe = _popen("nvidia-smi --query-gpu=compute_cap --format=csv,noheader,nounits 2>nul", "r");
#else
  FILE* pipe = popen("nvidia-smi --query-gpu=compute_cap --format=csv,noheader,nounits 2>/dev/null", "r");
#endif

  if (!pipe) {
    return false;
  }

  char buffer[128];
  std::string result;
  while (fgets(buffer, sizeof(buffer), pipe)) {
    result += buffer;
  }

#ifdef _WIN32
  int exit_code = _pclose(pipe);
#else
  int exit_code = pclose(pipe);
#endif

  if (exit_code != 0 || result.empty()) {
    return false;
  }

  // Need compute capability >= 5.0 for CUDA 12
  try {
    float compute_cap = std::stof(result);
    return compute_cap >= 5.0f;
  } catch (...) {
    return false;
  }
}

}  // namespace fl
