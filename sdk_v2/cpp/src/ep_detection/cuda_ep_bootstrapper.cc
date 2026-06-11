// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "ep_detection/cuda_ep_bootstrapper.h"

#include "ep_detection/ep_utils.h"
#include "http/http_client.h"
#include "http/http_download.h"
#include "logger.h"
#include "util/file_lock.h"
#include "util/zip_extract.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
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

constexpr const char* kOrtPackageFileName = "cuda-ep-ort.zip";
constexpr const char* kCudaDepsPackageFileName = "cuda-ep-cuda-deps.zip";
constexpr const char* kLockFileName = "cuda-ep.lock";
constexpr const char* kStagingDirName = "cuda-ep-staging";
constexpr const char* kUserAgent = "FoundryLocal";
constexpr int kMaxInstallAttempts = 5;

// Manifest URL on the CDN — published by the CUDA EP upload pipeline.
constexpr const char* kManifestUrl =
    "https://foundrypackages-ffhrdhbxb7gpdreh.b02.azurefd.net/cuda_ep_dev.json";

// Install flow:
// 1. Fetch the manifest and check the existing cuda-ep directory against it.
// 2. If any package is stale, copy the existing cuda-ep directory into staging.
// 3. Use that staged copy as the base and download only the stale package(s).
// 4. Re-write version.json with the ORT version, verify the staged files, then
//    atomically rename staging into place.

// -----------------------------------------------------------------------
// Platform detection
//
// Returns the manifest platform key and ORT registration library filename
// for the current build target, or std::nullopt if unsupported.
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
  struct PackageInfo {
    std::string download_url;
    std::unordered_map<std::string, std::string> sha256;  // filename -> expected hash
  };

  std::string ort_version;
  std::string cuda_deps_version;
  PackageInfo ort;
  PackageInfo cuda_deps;
};

ManifestInfo::PackageInfo ParsePackage(const nlohmann::json& package_json,
                                       const char* package_name) {
  ManifestInfo::PackageInfo info;

  info.download_url = package_json.at("url").get<std::string>();
  auto& sha256 = package_json.at("sha256");
  if (!sha256.is_object() || sha256.empty()) {
    throw std::runtime_error(
        fmt::format("CUDA EP manifest '{}' entry has invalid/empty 'sha256'", package_name));
  }

  for (auto& [filename, hash] : sha256.items()) {
    info.sha256[filename] = hash.get<std::string>();
  }

  return info;
}

bool DownloadAndExtractPackage(const ManifestInfo::PackageInfo& package,
                               const std::filesystem::path& staging_dir,
                               const std::filesystem::path& zip_path,
                               const std::string& package_name,
                               const std::string& ep_name,
                               const fl::IEpBootstrapper::ProgressCallback& progress_cb,
                               float progress_base,
                               float progress_span,
                               fl::ILogger& logger) {
  logger.Log(fl::LogLevel::Information,
             fmt::format("CUDA EP: downloading {} package...", package_name));
  logger.Log(fl::LogLevel::Debug,
             fmt::format("CUDA EP: {} download URL is {}", package_name, package.download_url));

  std::atomic<bool> cancel_flag{false};
  auto download_progress = [&](float pct) {
    if (progress_cb) {
      if (!progress_cb(ep_name, progress_base + (pct * progress_span))) {
        cancel_flag.store(true);
      }
    }
  };

  if (!HttpDownloadFile(package.download_url, zip_path, kUserAgent,
                        &cancel_flag, download_progress, logger)) {
    logger.Log(fl::LogLevel::Warning,
               fmt::format("CUDA EP: {} package download failed", package_name));
    return false;
  }

  logger.Log(fl::LogLevel::Information,
             fmt::format("CUDA EP: extracting {} package to {}",
                         package_name, staging_dir.string()));

  if (!ExtractZip(zip_path, staging_dir, logger)) {
    logger.Log(fl::LogLevel::Warning,
               fmt::format("CUDA EP: {} package extraction failed", package_name));
    return false;
  }

  std::filesystem::remove(zip_path);
  return true;
};

void WriteVersionJson(const std::filesystem::path& staging_dir,
                      const std::string& ort_version,
                      fl::ILogger& logger) {
  auto version_path = staging_dir / "version.json";
  auto version_json = nlohmann::json{{"version", ort_version}};

  std::ofstream out(version_path, std::ios::trunc | std::ios::binary);
  if (!out) {
    throw std::runtime_error(
        fmt::format("CUDA EP: failed to open {} for writing", version_path.string()));
  }

  out << version_json.dump();
  if (!out) {
    throw std::runtime_error(
        fmt::format("CUDA EP: failed to write {}", version_path.string()));
  }

  logger.Log(fl::LogLevel::Debug,
             fmt::format("CUDA EP: wrote version.json with ort_version={}", ort_version));
}

/// Fetch and parse the CUDA EP manifest from the CDN.
/// Returns the package entry for the given platform key.
ManifestInfo FetchManifest(const char* platform_key, fl::ILogger& logger) {
  logger.Log(fl::LogLevel::Debug,
             fmt::format("CUDA EP: fetching manifest from {}", kManifestUrl));

  auto body = fl::http::HttpGetWithRetry(kManifestUrl, kUserAgent, logger);
  auto j = nlohmann::json::parse(body);

  ManifestInfo info;
  info.ort_version = j.at("ort_version").get<std::string>();
  info.cuda_deps_version = j.at("cuda_deps_version").get<std::string>();

  auto& packages = j.at("packages");
  if (!packages.contains(platform_key)) {
    throw std::runtime_error(
        fmt::format("CUDA EP manifest has no entry for platform '{}'", platform_key));
  }

  auto& platform_package = packages.at(platform_key);
  if (!platform_package.contains("ort") || !platform_package.contains("cuda_deps")) {
    throw std::runtime_error(
        fmt::format("CUDA EP manifest platform '{}' is missing 'ort' or 'cuda_deps'",
                    platform_key));
  }

  info.ort = ParsePackage(platform_package.at("ort"), "ort");
  info.cuda_deps = ParsePackage(platform_package.at("cuda_deps"), "cuda_deps");

  return info;
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
               fmt::format("CUDA EP: manifest fetched (ort_version={}, cuda_deps_version={}, platform={})",
                           manifest.ort_version, manifest.cuda_deps_version, platform_info->key));

    // Cross-process lock to prevent concurrent installs.
    std::filesystem::create_directories(parent_dir);
    FileLock lock(parent_dir / kLockFileName);

    bool needs_ort = force || !VerifyEpPackage(ep_dir, manifest.ort.sha256, "CUDA EP (ort)", logger);
    bool needs_cuda_deps = force || !VerifyEpPackage(ep_dir, manifest.cuda_deps.sha256, "CUDA EP (cuda_deps)", logger);

    if (!needs_ort && !needs_cuda_deps) {
      logger.Log(LogLevel::Information,
                 "CUDA EP: ORT and CUDA deps packages already valid, skipping download");
    } else {
      // Download only outdated package(s) into staging, then atomically swap.
      auto staging_dir = parent_dir / kStagingDirName;
      if (std::filesystem::exists(staging_dir)) {
        std::filesystem::remove_all(staging_dir);
      }

      if (std::filesystem::exists(ep_dir)) {
        std::filesystem::copy(ep_dir, staging_dir,
                              std::filesystem::copy_options::recursive |
                              std::filesystem::copy_options::overwrite_existing);
      } else {
        std::filesystem::create_directories(staging_dir);
      }

      const int download_count = (needs_ort ? 1 : 0) + (needs_cuda_deps ? 1 : 0);
      float progress_base = 0.0f;
      const float progress_span = download_count > 0 ? (80.0f / download_count) : 0.0f;

      if (needs_ort) {
        auto ort_zip_path = staging_dir / kOrtPackageFileName;
        if (!DownloadAndExtractPackage(manifest.ort, staging_dir, ort_zip_path,
                                       "ort", name_, progress_cb,
                                       progress_base, progress_span, logger)) {
          std::filesystem::remove_all(staging_dir);
          return false;
        }
        progress_base += progress_span;
      }

      if (needs_cuda_deps) {
        auto cuda_deps_zip_path = staging_dir / kCudaDepsPackageFileName;
        if (!DownloadAndExtractPackage(manifest.cuda_deps, staging_dir, cuda_deps_zip_path,
                                       "cuda_deps", name_, progress_cb,
                                       progress_base, progress_span, logger)) {
          std::filesystem::remove_all(staging_dir);
          return false;
        }
        progress_base += progress_span;
      }

      // CUDA has two install steps (ORT package and CUDA deps), so always stamp
      // the final package with the ORT version after both steps complete.
      WriteVersionJson(staging_dir, manifest.ort_version, logger);

      // Verify both package subsets in staging before promotion.
      if (!VerifyEpPackage(staging_dir, manifest.ort.sha256, "CUDA EP (ort)", logger) ||
          !VerifyEpPackage(staging_dir, manifest.cuda_deps.sha256, "CUDA EP (cuda_deps)", logger)) {
        logger.Log(LogLevel::Warning,
                   "CUDA EP: verification failed after downloading updated package(s)");
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

      logger.Log(LogLevel::Information,
                 fmt::format("CUDA EP: successfully installed (updated: ort={}, cuda_deps={})",
                             needs_ort ? "yes" : "no",
                             needs_cuda_deps ? "yes" : "no"));
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
               fmt::format("CUDA EP: ready (install_path={}, ort_version={}, cuda_deps_version={})",
                           ep_dir.string(), manifest.ort_version, manifest.cuda_deps_version));
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
