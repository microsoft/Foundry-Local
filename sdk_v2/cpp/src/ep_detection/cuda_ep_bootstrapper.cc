// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "ep_detection/cuda_ep_bootstrapper.h"

#include "logger.h"
#include "util/file_lock.h"
#include "http/http_download.h"
#include "util/sha256.h"
#include "util/zip_extract.h"

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {

constexpr const char* kPackageFileName = "cuda-ep.zip";
constexpr const char* kLockFileName = "cuda-ep.lock";
constexpr const char* kUserAgent = "FoundryLocal";
constexpr int kMaxInstallAttempts = 5;

// CUDA EP package is built against the ONNX Runtime version we link against, so
// WinML and non-WinML builds need separate downloads. Hashes mirror the C# core
// (see neutron.main/src/Service/Providers/Detector/CudaEpBootstrapper.cs).
//   WinML build  -> ORT 1.23.2 (cuda-ep-20260501-182408.zip)
//   Non-WinML    -> ORT 1.25.1 (cuda-ep-20260501-062935.zip)
#if defined(FOUNDRY_LOCAL_USE_WINML) && FOUNDRY_LOCAL_USE_WINML
constexpr const char* kDownloadUrl =
    "https://foundrypackages-ffhrdhbxb7gpdreh.b02.azurefd.net/cuda-ep-20260501-182408.zip";
#else
constexpr const char* kDownloadUrl =
    "https://foundrypackages-ffhrdhbxb7gpdreh.b02.azurefd.net/cuda-ep-20260501-062935.zip";
#endif

struct ExpectedBinary {
  const char* filename;
  const char* sha256;
};

bool VerifyCudaPackage(
    const std::filesystem::path& dir,
    std::initializer_list<std::pair<std::string_view, std::string_view>> expected,
    fl::ILogger& logger) {
  for (const auto& [filename, expected_hash] : expected) {
    auto file_path = dir / filename;

    if (!std::filesystem::exists(file_path)) {
      return false;
    }

    auto hash = fl::Sha256File(file_path);

    // Case-insensitive hex comparison
    if (hash.size() != expected_hash.size() ||
        !std::equal(hash.begin(), hash.end(), expected_hash.begin(),
                    [](char a, char b) {
                      return std::toupper(static_cast<unsigned char>(a)) ==
                             std::toupper(static_cast<unsigned char>(b));
                    })) {
      logger.Log(fl::LogLevel::Warning,
                 fmt::format("CUDA EP: hash mismatch for {}: got {}, expected {}",
                             filename, hash, expected_hash));
      return false;
    }
  }

  return true;
}

#if defined(FOUNDRY_LOCAL_USE_WINML) && FOUNDRY_LOCAL_USE_WINML
constexpr ExpectedBinary kExpectedBinaries[] = {
    {"onnxruntime_providers_cuda.dll", "4CEF18654878CEFCFCF8488E9C3A705EB5327AA9B5556155C319C9CBB2D98FCF"},
    {"onnxruntime-genai-cuda.dll", "BC953F8E2AAFC6219B2D723B65AB8F1A9426A6B7724D6A01ED756FAE8C3DE6AE"},
};
#else
constexpr ExpectedBinary kExpectedBinaries[] = {
    {"onnxruntime_providers_cuda.dll", "DD540FCFECFBC68B4675C9ADF09C2858CF6B054563859D79598AA2524406A76F"},
    {"onnxruntime-genai-cuda.dll", "BC953F8E2AAFC6219B2D723B65AB8F1A9426A6B7724D6A01ED756FAE8C3DE6AE"},
};
#endif

constexpr const char* kRegistrationName = "Foundry.CUDA";
constexpr const char* kCudaProviderDll = "onnxruntime_providers_cuda.dll";

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
    // Cross-process lock to prevent concurrent installs
    FileLock lock(lock_path);

    // Check if package already exists and is valid
        if (VerifyCudaPackage(ep_dir,
          {{kExpectedBinaries[0].filename, kExpectedBinaries[0].sha256},
           {kExpectedBinaries[1].filename, kExpectedBinaries[1].sha256}},
          logger)) {
      logger.Log(LogLevel::Information, "CUDA EP: package already valid, skipping download");
    } else {
      // Clean up any partial install
      if (std::filesystem::exists(ep_dir)) {
        std::filesystem::remove_all(ep_dir);
      }

      std::filesystem::create_directories(ep_dir);

      // Download
      logger.Log(LogLevel::Information, "CUDA EP: downloading from CDN...");

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

      if (!HttpDownloadFile(kDownloadUrl, zip_path, kUserAgent,
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
      if (!VerifyCudaPackage(ep_dir,
               {{kExpectedBinaries[0].filename, kExpectedBinaries[0].sha256},
                {kExpectedBinaries[1].filename, kExpectedBinaries[1].sha256}},
               logger)) {
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

    auto cuda_dll_path = ep_dir / kCudaProviderDll;

    if (!register_ep_(kRegistrationName, cuda_dll_path)) {
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
