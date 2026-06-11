// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "ep_detection/webgpu_ep_bootstrapper.h"

#include "ep_detection/ep_utils.h"
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
#include <filesystem>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {

constexpr const char* kPackageFileName = "webgpu-ep.zip";
constexpr const char* kLockFileName = "webgpu-ep.lock";
constexpr const char* kStagingDirName = "webgpu-ep-staging";
constexpr const char* kUserAgent = "FoundryLocal";
constexpr int kMaxInstallAttempts = 5;

// Manifest URL — always uses prod.
constexpr const char* kManifestUrl =
    "https://foundrypackages-ffhrdhbxb7gpdreh.b02.azurefd.net/webgpu_ep_prod.json";

// Platform key used to look up this platform's package in the manifest.
#if defined(_WIN32) && defined(_M_ARM64)
constexpr const char* kPlatformKey = "win-arm64";
#elif defined(_WIN32)
constexpr const char* kPlatformKey = "win-x64";
#elif defined(__APPLE__)
constexpr const char* kPlatformKey = "macos-arm64";
#else
constexpr const char* kPlatformKey = "linux-x64";
#endif

// Platform-specific EP library filename.
#if defined(_WIN32)
constexpr const char* kWebGpuProviderLib = "onnxruntime_providers_webgpu.dll";
#elif defined(__APPLE__)
constexpr const char* kWebGpuProviderLib = "libonnxruntime_providers_webgpu.dylib";
#else
constexpr const char* kWebGpuProviderLib = "libonnxruntime_providers_webgpu.so";
#endif

constexpr const char* kRegistrationName = "Foundry.WebGPU";

/// Parsed manifest entry for a single platform.
struct ManifestPackageInfo {
  std::string url;
  std::string sha256;  // expected SHA256 hash of kWebGpuProviderLib
};

/// Fetch the manifest JSON from CDN and extract the package info for this platform.
ManifestPackageInfo FetchManifest(fl::ILogger& logger) {
  logger.Log(fl::LogLevel::Debug, fmt::format("WebGPU EP: fetching manifest from {}", kManifestUrl));

  auto body = fl::http::HttpGetWithRetry(kManifestUrl, kUserAgent, logger);
  auto manifest = nlohmann::json::parse(body);

  if (!manifest.contains("packages") || !manifest["packages"].is_object()) {
    throw std::runtime_error(
        fmt::format("WebGPU EP: manifest is invalid — missing 'packages' field. "
                    "Raw content (first 200 chars): {}",
                    body.substr(0, 200)));
  }

  const auto& packages = manifest["packages"];
  if (!packages.contains(kPlatformKey)) {
    std::string available;
    for (auto it = packages.begin(); it != packages.end(); ++it) {
      if (!available.empty()) available += ", ";
      available += it.key();
    }
    throw std::runtime_error(
        fmt::format("WebGPU EP: manifest does not contain a package for platform '{}'. "
                    "Available platforms: {}",
                    kPlatformKey, available));
  }

  const auto& pkg = packages[kPlatformKey];
  ManifestPackageInfo info;
  info.url = pkg.at("url").get<std::string>();
  info.sha256 = pkg.at("sha256").at(kWebGpuProviderLib).get<std::string>();

  logger.Log(fl::LogLevel::Information,
             fmt::format("WebGPU EP: manifest fetched for platform '{}'", kPlatformKey));
  return info;
}

}  // anonymous namespace

namespace fl {

WebGpuEpBootstrapper::WebGpuEpBootstrapper(std::string ep_dir, EpRegistrationCallback register_ep)
    : ep_dir_(std::move(ep_dir)), register_ep_(std::move(register_ep)) {}

const std::string& WebGpuEpBootstrapper::Name() const {
  return name_;
}

bool WebGpuEpBootstrapper::IsRegistered() const {
  return registered_;
}

bool WebGpuEpBootstrapper::DownloadAndRegister(bool force,
                                               const ProgressCallback& progress_cb,
                                               ILogger& logger) {
  if (registered_ && !force) {
    if (progress_cb) {
      progress_cb(name_, 100.0f);
    }
    return true;
  }

  if (!force && attempts_ >= kMaxInstallAttempts) {
    logger.Log(LogLevel::Warning, "WebGPU EP: max install attempts reached");
    return false;
  }

  attempts_++;

  auto ep_dir = std::filesystem::path(ep_dir_);
  auto parent_dir = ep_dir.parent_path();

  try {
    // Fetch manifest before acquiring lock (avoid holding lock during network I/O)
    auto manifest = FetchManifest(logger);

    // Check if package already exists and is valid
    if (!force && VerifyEpPackage(ep_dir, {{kWebGpuProviderLib, manifest.sha256}}, "WebGPU EP", logger)) {
      logger.Log(LogLevel::Debug, "WebGPU EP: local binaries match manifest, skipping download");
    } else {
      // Ensure parent directory exists for the lock file
      std::filesystem::create_directories(parent_dir);
      auto lock_path = parent_dir / kLockFileName;

      // Cross-process lock to prevent concurrent installs
      FileLock lock(lock_path);

      // Re-check after acquiring lock (another process may have completed the update)
      if (!force && VerifyEpPackage(ep_dir, {{kWebGpuProviderLib, manifest.sha256}}, "WebGPU EP", logger)) {
        logger.Log(LogLevel::Debug, "WebGPU EP: another process already completed the update");
      } else {
        // Download and extract to staging directory for atomic swap
        auto staging_dir = parent_dir / kStagingDirName;
        if (std::filesystem::exists(staging_dir)) {
          std::filesystem::remove_all(staging_dir);
        }
        std::filesystem::create_directories(staging_dir);

        auto zip_path = staging_dir / kPackageFileName;

        // Download
        logger.Log(LogLevel::Information,
                   fmt::format("WebGPU EP: downloading for {} from CDN", kPlatformKey));
        logger.Log(LogLevel::Debug,
                   fmt::format("WebGPU EP: download URL is {}", manifest.url));

        std::atomic<bool> cancel_flag{false};
        auto download_progress = [&](float pct) {
          if (progress_cb) {
            // 0–80% for download phase
            if (!progress_cb(name_, pct * 0.8f)) {
              cancel_flag.store(true);
            }
          }
        };

        if (!HttpDownloadFile(manifest.url, zip_path, kUserAgent,
                              &cancel_flag, download_progress, logger)) {
          logger.Log(LogLevel::Warning, "WebGPU EP: download failed (see prior log for details)");
          return false;
        }

        // Extract
        logger.Log(LogLevel::Information,
                   fmt::format("WebGPU EP: extracting package to {}", staging_dir.string()));

        if (!ExtractZip(zip_path, staging_dir, logger)) {
          logger.Log(LogLevel::Warning, "WebGPU EP: extraction failed");
          std::filesystem::remove_all(staging_dir);
          return false;
        }

        // Clean up zip
        std::filesystem::remove(zip_path);

        // Verify staging
        if (!VerifyEpPackage(staging_dir, {{kWebGpuProviderLib, manifest.sha256}}, "WebGPU EP", logger)) {
          logger.Log(LogLevel::Warning,
                     fmt::format("WebGPU EP: verification failed after extraction (attempt {})",
                                 attempts_));
          std::filesystem::remove_all(staging_dir);
          return false;
        }

        logger.Log(LogLevel::Debug,
                   fmt::format("WebGPU EP: staging verification succeeded, promoting to {}",
                               ep_dir.string()));

        // Atomic swap: delete old, rename staging to target
        if (std::filesystem::exists(ep_dir)) {
          std::filesystem::remove_all(ep_dir);
        }
        std::filesystem::rename(staging_dir, ep_dir);

        logger.Log(LogLevel::Information, "WebGPU EP: successfully installed");
      }
    }

    if (progress_cb) {
      progress_cb(name_, 90.0f);
    }

    // Register with ORT
#ifdef _WIN32
    // Prepend the EP directory to PATH for the process lifetime.
    // WebGPU EP may delay-load additional dependencies from the same directory.
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

    auto provider_path = ep_dir / kWebGpuProviderLib;

    if (!register_ep_(kRegistrationName, provider_path)) {
      logger.Log(LogLevel::Warning, "WebGPU EP: ORT registration failed");
      return false;
    }

    registered_ = true;

    if (progress_cb) {
      progress_cb(name_, 100.0f);
    }

    logger.Log(LogLevel::Information,
               fmt::format("WebGPU EP: ready (install_path={})", ep_dir.string()));
    return true;
  } catch (const std::exception& e) {
    logger.Log(LogLevel::Warning, fmt::format("WebGPU EP: error: {}", e.what()));
    return false;
  }
}

}  // namespace fl
