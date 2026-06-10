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
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {

constexpr const char* kPackageFileName = "cuda-ep.zip";
constexpr const char* kLockFileName = "cuda-ep.lock";
constexpr const char* kStagingDirName = "cuda-ep-staging";
constexpr const char* kUserAgent = "FoundryLocal";
constexpr int kMaxInstallAttempts = 5;

// Manifest URL on the CDN — published by the CUDA EP upload pipeline.
constexpr const char* kManifestUrl =
    "https://foundrypackages-ffhrdhbxb7gpdreh.b02.azurefd.net/cuda_ep_prod.json";

// -----------------------------------------------------------------------
// Platform detection
//
// Returns the manifest platform key and ORT registration library filename
// for the current build target, or std::nullopt if unsupported.
//
// To add a platform:
//   1. Uncomment its #elif block below.
//   2. Uncomment its entry in $binaryNames / $expectedPlatforms in
//      cuda-ep-upload.yml and update $platformPattern there too.
// -----------------------------------------------------------------------
struct PlatformInfo {
  const char* key;     // manifest lookup key, e.g. "win-x64"
  const char* ep_lib;  // ORT registration library filename
};

std::optional<PlatformInfo> GetPlatformInfo() {
#if defined(_WIN32) && !defined(_M_ARM64)
  return PlatformInfo{"win-x64", "onnxruntime_providers_cuda_plugin.dll"};

// Uncomment when win-arm64 CUDA EP build is available (see cuda-ep-upload.yml):
// #elif defined(_WIN32) && defined(_M_ARM64)
//   return PlatformInfo{"win-arm64", "onnxruntime_providers_cuda_plugin.dll"};

// Uncomment when linux-x64 CUDA EP build is available (see cuda-ep-upload.yml):
// #elif defined(__linux__) && defined(__x86_64__)
//   return PlatformInfo{"linux-x64", "libonnxruntime_providers_cuda_plugin.so"};

// Uncomment when linux-arm64 CUDA EP build is available (see cuda-ep-upload.yml):
// #elif defined(__linux__) && defined(__aarch64__)
//   return PlatformInfo{"linux-arm64", "libonnxruntime_providers_cuda_plugin.so"};

#else
  return std::nullopt;  // Platform not yet supported — graceful no-op.
#endif
}

constexpr const char* kRegistrationName = "Foundry.CUDA";

struct ManifestInfo {
  std::string version;
  std::string download_url;
  std::unordered_map<std::string, std::string> sha256;  // filename -> expected hash
};

/// Fetch and parse the CUDA EP manifest from the CDN.
/// Returns the package entry for the given platform key.
ManifestInfo FetchManifest(const char* platform_key, fl::ILogger& logger) {
  logger.Log(fl::LogLevel::Debug,
             fmt::format("CUDA EP: fetching manifest from {}", kManifestUrl));

  auto body = fl::http::HttpGetWithRetry(kManifestUrl, kUserAgent, logger);
  auto j = nlohmann::json::parse(body);

  ManifestInfo info;
  info.version = j.at("version").get<std::string>();

  auto& packages = j.at("packages");
  if (!packages.contains(platform_key)) {
    throw std::runtime_error(
        fmt::format("CUDA EP manifest has no entry for platform '{}'", platform_key));
  }

  auto& pkg = packages.at(platform_key);
  info.download_url = pkg.at("url").get<std::string>();

  for (auto& [filename, hash] : pkg.at("sha256").items()) {
    info.sha256[filename] = hash.get<std::string>();
  }

  return info;
}

/// Verify all expected binaries exist and have correct SHA256 hashes.
/// Logs the name of the first missing or mismatched file to aid diagnosis.
bool VerifyPackage(const std::filesystem::path& dir,
                   const std::unordered_map<std::string, std::string>& expected_hashes,
                   fl::ILogger& logger) {
  // Quick sentinel check before the expensive SHA256 work.
  if (!std::filesystem::exists(dir)) {
    logger.Log(fl::LogLevel::Debug,
               fmt::format("CUDA EP: package directory does not exist: {}", dir.string()));
    return false;
  }

  if (expected_hashes.empty()) {
    logger.Log(fl::LogLevel::Warning, "CUDA EP: manifest contains no expected SHA256 hashes");
    return false;
  }

  for (const auto& [filename, expected_hash] : expected_hashes) {
    auto file_path = dir / filename;

    if (!std::filesystem::exists(file_path)) {
      logger.Log(fl::LogLevel::Debug,
                 fmt::format("CUDA EP: package file missing: {}", file_path.string()));
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

  // Bail out early if this platform is not yet in the manifest.
  auto platform_info = GetPlatformInfo();
  if (!platform_info) {
    logger.Log(LogLevel::Information, "CUDA EP: current platform is not yet supported");
    return false;
  }

  auto ep_dir = std::filesystem::path(ep_dir_);
  auto parent_dir = ep_dir.parent_path();

  try {
    // Fetch the manifest before acquiring the lock to avoid holding it during network I/O.
    auto manifest = FetchManifest(platform_info->key, logger);
    logger.Log(LogLevel::Information,
               fmt::format("CUDA EP: manifest fetched (version={}, platform={})",
                           manifest.version, platform_info->key));

    // Cross-process lock to prevent concurrent installs.
    std::filesystem::create_directories(parent_dir);
    FileLock lock(parent_dir / kLockFileName);

    // Re-check after acquiring the lock — another process may have already updated.
    if (!force && VerifyPackage(ep_dir, manifest.sha256, logger)) {
      logger.Log(LogLevel::Information, "CUDA EP: package already valid, skipping download");
    } else {
      // Download to a staging directory so a failure never corrupts the existing install.
      auto staging_dir = parent_dir / kStagingDirName;
      if (std::filesystem::exists(staging_dir)) {
        std::filesystem::remove_all(staging_dir);
      }
      std::filesystem::create_directories(staging_dir);

      auto zip_path = staging_dir / kPackageFileName;

      logger.Log(LogLevel::Information,
                 fmt::format("CUDA EP: downloading for {}...", platform_info->key));
      logger.Log(LogLevel::Debug,
                 fmt::format("CUDA EP: download URL is {}", manifest.download_url));

      std::atomic<bool> cancel_flag{false};
      auto download_progress = [&](float pct) {
        if (progress_cb) {
          // 0–80% for the download phase.
          if (!progress_cb(name_, pct * 0.8f)) {
            cancel_flag.store(true);
          }
        }
      };

      if (!HttpDownloadFile(manifest.download_url, zip_path, kUserAgent,
                            &cancel_flag, download_progress, logger)) {
        logger.Log(LogLevel::Warning, "CUDA EP: download failed (see prior log for details)");
        std::filesystem::remove_all(staging_dir);
        return false;
      }

      logger.Log(LogLevel::Information,
                 fmt::format("CUDA EP: extracting package to {}", staging_dir.string()));

      if (!ExtractZip(zip_path, staging_dir, logger)) {
        logger.Log(LogLevel::Warning, "CUDA EP: extraction failed");
        std::filesystem::remove_all(staging_dir);
        return false;
      }

      std::filesystem::remove(zip_path);

      if (!VerifyPackage(staging_dir, manifest.sha256, logger)) {
        logger.Log(LogLevel::Warning, "CUDA EP: verification failed after extraction");
        std::filesystem::remove_all(staging_dir);
        return false;
      }

      logger.Log(LogLevel::Debug,
                 fmt::format("CUDA EP: staging verification succeeded, promoting to {}",
                             ep_dir.string()));

      // Atomic swap: delete old install, rename staging to target.
      if (std::filesystem::exists(ep_dir)) {
        std::filesystem::remove_all(ep_dir);
      }
      std::filesystem::rename(staging_dir, ep_dir);
      logger.Log(LogLevel::Information, "CUDA EP: successfully installed.");
    }

    if (progress_cb) {
      progress_cb(name_, 90.0f);
    }

    // Register with ORT.
#ifdef _WIN32
    // Permanently prepend the EP directory to PATH. The zip bundles all
    // required CUDA/cuDNN DLLs, so no system CUDA install is needed.
    // PATH must stay modified for the process lifetime because:
    //   - onnxruntime_providers_cuda_plugin.dll delay-loads CUDA dependencies
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

    auto cuda_lib_path = ep_dir / platform_info->ep_lib;

    // NOTE: RegisterExecutionProviderLibrary loads the CUDA plugin DLL, which
    // initializes the CUDA runtime and cuDNN. This can take 30–60 seconds on
    // first use — especially on machines with large cuDNN caches or slow VRAM
    // init. This is normal; it is NOT a hang in the bootstrapper itself.
    logger.Log(LogLevel::Information,
               fmt::format("CUDA EP: registering provider library {} (CUDA init may take ~30s)...",
                           cuda_lib_path.string()));

    if (!register_ep_(kRegistrationName, cuda_lib_path)) {
      logger.Log(LogLevel::Warning, "CUDA EP: ORT registration failed");
      return false;
    }

    registered_ = true;

    if (progress_cb) {
      progress_cb(name_, 100.0f);
    }

    logger.Log(LogLevel::Information,
               fmt::format("CUDA EP: ready (install_path={}, version={})",
                           ep_dir.string(), manifest.version));
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
