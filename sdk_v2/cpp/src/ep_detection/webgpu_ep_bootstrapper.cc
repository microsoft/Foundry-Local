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
#include <fstream>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {

constexpr const char* kPackageFileName = "webgpu-ep.nupkg";
constexpr const char* kVersionFileName = "webgpu-ep.version";
constexpr const char* kLockFileName = "webgpu-ep.lock";
constexpr const char* kStagingDirName = "webgpu-ep-staging";
constexpr const char* kUserAgent = "FoundryLocal";
constexpr int kMaxInstallAttempts = 5;
constexpr const char* kNuGetServiceIndex = "https://api.nuget.org/v3/index.json";
// NuGet v3 flat-container URLs require lowercase package IDs in the path.
constexpr const char* kWebGpuPackageId = "microsoft.ml.onnxruntime.ep.webgpu";

// Platform-specific EP library filename.
#if defined(_WIN32)
constexpr const char* kWebGpuProviderLib = "onnxruntime_providers_webgpu.dll";
#elif defined(__APPLE__)
constexpr const char* kWebGpuProviderLib = "libonnxruntime_providers_webgpu.dylib";
#else
constexpr const char* kWebGpuProviderLib = "libonnxruntime_providers_webgpu.so";
#endif

// Platform-specific runtime RID inside NuGet package paths: runtimes/{rid}/native.
#if defined(_WIN32) && defined(_M_ARM64)
constexpr const char* kWebGpuRuntimeRid = "win-arm64";
#elif defined(_WIN32)
constexpr const char* kWebGpuRuntimeRid = "win-x64";
#elif defined(__APPLE__)
constexpr const char* kWebGpuRuntimeRid = "osx-arm64";
#else
constexpr const char* kWebGpuRuntimeRid = "linux-x64";
#endif

constexpr const char* kRegistrationName = "Foundry.WebGPU";

struct NuGetPackageInfo {
  std::string version;
  std::string url;
};

std::string GetPackageBaseAddress(fl::ILogger& logger) {
  auto body = fl::http::HttpGetWithRetry(kNuGetServiceIndex, kUserAgent, logger);
  auto index = nlohmann::json::parse(body);

  if (!index.contains("resources") || !index["resources"].is_array()) {
    throw std::runtime_error("WebGPU EP: NuGet service index missing resources array");
  }

  for (const auto& resource : index["resources"]) {
    if (!resource.contains("@id") || !resource.contains("@type")) {
      continue;
    }

    const auto type = resource.at("@type").dump();
    if (type.find("PackageBaseAddress") == std::string::npos) {
      continue;
    }

    auto base = resource.at("@id").get<std::string>();
    if (!base.empty() && base.back() == '/') {
      base.pop_back();
    }
    return base;
  }

  throw std::runtime_error("WebGPU EP: NuGet service index missing package base address resource");
}

std::string ReadInstalledVersion(const std::filesystem::path& ep_dir) {
  std::ifstream stream(ep_dir / kVersionFileName);
  std::string version;
  std::getline(stream, version);
  return version;
}

NuGetPackageInfo ResolveWebGpuPackage(fl::ILogger& logger) {
  auto package_base = GetPackageBaseAddress(logger);

  auto versions_url = fmt::format("{}/{}/index.json", package_base, kWebGpuPackageId);
  auto versions_body = fl::http::HttpGetWithRetry(versions_url, kUserAgent, logger);
  auto versions_doc = nlohmann::json::parse(versions_body);

  if (!versions_doc.contains("versions") || !versions_doc["versions"].is_array()) {
    throw std::runtime_error("WebGPU EP: NuGet versions document missing versions array");
  }

  std::string latest_stable;
  for (const auto& version_entry : versions_doc["versions"]) {
    auto version = version_entry.get<std::string>();
    // Prefer the latest stable version
    if (version.find('-') == std::string_view::npos) {
      latest_stable = std::move(version);
    }
  }

  if (latest_stable.empty()) {
    throw std::runtime_error("WebGPU EP: no stable NuGet package version found");
  }

  return NuGetPackageInfo{
      latest_stable,
      fmt::format("{}/{}/{}/{}.{}.nupkg",
                  package_base,
            kWebGpuPackageId,
                  latest_stable,
            kWebGpuPackageId,
                  latest_stable)};
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
    auto package = ResolveWebGpuPackage(logger);

    if (!force && std::filesystem::exists(ep_dir / kWebGpuProviderLib) &&
        ReadInstalledVersion(ep_dir) == package.version) {
      logger.Log(LogLevel::Debug,
                 fmt::format("WebGPU EP: local runtime already present at version {}",
                             package.version));
    } else {
      // Ensure parent directory exists for the lock file
      std::filesystem::create_directories(parent_dir);
      auto lock_path = parent_dir / kLockFileName;

      // Cross-process lock to prevent concurrent installs
      FileLock lock(lock_path);

      // Re-check after acquiring lock (another process may have completed the update)
      if (!force && std::filesystem::exists(ep_dir / kWebGpuProviderLib) &&
          ReadInstalledVersion(ep_dir) == package.version) {
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
                   fmt::format("WebGPU EP: downloading {} {} from NuGet",
                               kWebGpuPackageId, package.version));
        logger.Log(LogLevel::Debug,
                   fmt::format("WebGPU EP: download URL is {}", package.url));

        std::atomic<bool> cancel_flag{false};
        auto download_progress = [&](float pct) {
          if (progress_cb) {
            // 0–80% for download phase
            if (!progress_cb(name_, pct * 0.8f)) {
              cancel_flag.store(true);
            }
          }
        };

        if (!HttpDownloadFile(package.url, zip_path, kUserAgent,
                              &cancel_flag, download_progress, logger)) {
          logger.Log(LogLevel::Warning, "WebGPU EP: download failed (see prior log for details)");
          return false;
        }

        logger.Log(LogLevel::Information,
                   fmt::format("WebGPU EP: verifying signature and extracting runtime payload {} from {}",
                               kWebGpuRuntimeRid, zip_path.string()));

        if (!VerifyPackage(zip_path, kWebGpuRuntimeRid, staging_dir, logger)) {
          logger.Log(LogLevel::Warning,
                     "WebGPU EP: signature verification or extraction failed");
          std::filesystem::remove_all(staging_dir);
          return false;
        }

        std::filesystem::remove(zip_path);

        if (!std::filesystem::exists(staging_dir / kWebGpuProviderLib)) {
          logger.Log(LogLevel::Warning,
                     fmt::format("WebGPU EP: runtime payload missing {} after extraction",
                                 kWebGpuProviderLib));
          std::filesystem::remove_all(staging_dir);
          return false;
        }

        {
          std::ofstream version_stream(staging_dir / kVersionFileName, std::ios::trunc);
          version_stream << package.version;
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
