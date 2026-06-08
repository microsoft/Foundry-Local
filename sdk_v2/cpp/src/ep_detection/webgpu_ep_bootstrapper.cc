// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "ep_detection/webgpu_ep_bootstrapper.h"

#include "http/http_download.h"
#include "logger.h"
#include "util/file_lock.h"
#include "util/sha256.h"
#include "util/zip_extract.h"

#include <fmt/format.h>

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
constexpr const char* kUserAgent = "FoundryLocal";
constexpr int kMaxInstallAttempts = 5;

// Platform-specific download URL suffix.
#if defined(_WIN32) && defined(_M_ARM64)
constexpr const char* kPlatformSuffix = "win-arm64";
#elif defined(_WIN32)
constexpr const char* kPlatformSuffix = "win-x64";
#elif defined(__APPLE__)
constexpr const char* kPlatformSuffix = "macos-arm64";
#else
constexpr const char* kPlatformSuffix = "linux-x64";
#endif

// Platform-specific EP library filename and expected SHA-256 hash.
// -- Update these hashes when uploading new WebGPU EP binaries --
#if defined(_WIN32)
constexpr const char* kWebGpuProviderLib = "onnxruntime_providers_webgpu.dll";
#if defined(_M_ARM64)
constexpr const char* kWebGpuProviderHash =
    "3AE46E25A2DF149A890A78A09B466189070456EC79AC206E87E09F1840704597";
#else
constexpr const char* kWebGpuProviderHash =
    "8E074DB27BE59203A8F58E15E8700058D1F76DF7A4295EA3361FC46331BB985E";
#endif
#elif defined(__APPLE__)
constexpr const char* kWebGpuProviderLib = "libonnxruntime_providers_webgpu.dylib";
constexpr const char* kWebGpuProviderHash =
    "12D9E105FCAC11B50685DB64462D7490C7AEEB5219530387464A7CF6D9F323E7";
#else
constexpr const char* kWebGpuProviderLib = "libonnxruntime_providers_webgpu.so";
constexpr const char* kWebGpuProviderHash =
    "64211A7844B243DB78E0ECA0FAB7DF0EE5B4F7D131886A00C09CF105BF7D94CE";
#endif

struct ExpectedBinary {
  const char* filename;
  const char* sha256;
};

constexpr ExpectedBinary kExpectedBinaries[] = {
    {kWebGpuProviderLib, kWebGpuProviderHash},
};

constexpr const char* kRegistrationName = "Foundry.WebGPU";

/// Build the full CDN download URL for the current platform.
std::string GetDownloadUrl() {
  return fmt::format(
      "https://foundrypackages-ffhrdhbxb7gpdreh.b02.azurefd.net/webgpu_ep_20260504-224804_{}.zip",
      kPlatformSuffix);
}

/// Verify all expected binaries exist and have correct SHA256 hashes.
bool VerifyPackage(const std::filesystem::path& dir, fl::ILogger& logger) {
  for (const auto& expected : kExpectedBinaries) {
    auto file_path = dir / expected.filename;

    if (!std::filesystem::exists(file_path)) {
      return false;
    }

    auto hash = fl::Sha256File(file_path);

    // Case-insensitive comparison
    std::string expected_hash(expected.sha256);
    if (!std::equal(hash.begin(), hash.end(), expected_hash.begin(), expected_hash.end(),
                    [](char a, char b) { return std::toupper(a) == std::toupper(b); })) {
      logger.Log(fl::LogLevel::Warning,
                 fmt::format("WebGPU EP: hash mismatch for {}: got {}, expected {}",
                             expected.filename, hash, expected.sha256));
      return false;
    }
  }

  return true;
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
  auto lock_path = ep_dir.parent_path() / kLockFileName;
  auto zip_path = ep_dir.parent_path() / kPackageFileName;

  try {
    // Cross-process lock to prevent concurrent installs
    FileLock lock(lock_path);

    // Check if package already exists and is valid
    if (VerifyPackage(ep_dir, logger)) {
      logger.Log(LogLevel::Information, "WebGPU EP: package already valid, skipping download");
    } else {
      // Clean up any partial install
      if (std::filesystem::exists(ep_dir)) {
        std::filesystem::remove_all(ep_dir);
      }

      std::filesystem::create_directories(ep_dir);

      // Download
      auto url = GetDownloadUrl();
      logger.Log(LogLevel::Information, fmt::format("WebGPU EP: downloading from CDN ({})", url));

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

      if (!HttpDownloadFile(url, zip_path, kUserAgent,
                            &cancel_flag, download_progress, logger)) {
        logger.Log(LogLevel::Warning, "WebGPU EP: download failed (see prior log for details)");
        return false;
      }

      // Extract
      logger.Log(LogLevel::Information, "WebGPU EP: extracting...");

      if (!ExtractZip(zip_path, ep_dir, logger)) {
        logger.Log(LogLevel::Warning, "WebGPU EP: extraction failed");
        return false;
      }

      // Clean up zip
      std::filesystem::remove(zip_path);

      // Verify
      if (!VerifyPackage(ep_dir, logger)) {
        logger.Log(LogLevel::Warning, "WebGPU EP: verification failed after download");
        return false;
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

    // Bootstrapper-side log — captures the install dir, which the central
    // register_ep callback (logs library + version) doesn't have.
    logger.Log(LogLevel::Information,
               fmt::format("WebGPU EP: ready (install_path={})", ep_dir.string()));
    return true;
  } catch (const std::exception& e) {
    logger.Log(LogLevel::Warning, fmt::format("WebGPU EP: error: {}", e.what()));
    return false;
  }
}

}  // namespace fl
