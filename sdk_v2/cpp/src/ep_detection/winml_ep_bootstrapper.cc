// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// This translation unit is only compiled when FOUNDRY_LOCAL_USE_WINML=ON and
// the WinML EP catalog NuGet package was resolved at CMake time. See
// sdk_v2/cpp/CMakeLists.txt for the source-list gating. The corresponding
// header (and this file) unconditionally reference WinML 2.x catalog APIs.
#include "ep_detection/winml_ep_bootstrapper.h"

#include "logger.h"

#include <fmt/format.h>

#include <string>
#include <vector>

// WinML EP Catalog C API — delay-loaded via Microsoft.Windows.AI.MachineLearning.dll
#include <WinMLEpCatalog.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace fl {

namespace {

/// Look up the running Windows build number via ``ntdll!RtlGetVersion``. We
/// use ``GetProcAddress`` instead of including ``<winternl.h>`` to avoid the
/// header's macro pollution; the function signature is stable and documented.
/// ``RtlGetVersion`` accepts an ``OSVERSIONINFOW`` (same struct layout as
/// ``RTL_OSVERSIONINFOW`` — both are typedef aliases in ``<winnt.h>``) so
/// callers don't need any ``Rtl``-specific headers either.
/// Returns 0 if the lookup fails (purely diagnostic — never gates behavior).
DWORD QueryWindowsBuild() {
  using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
  HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
  if (!ntdll) {
    return 0;
  }
  auto rtl_get_version = reinterpret_cast<RtlGetVersionFn>(
      ::GetProcAddress(ntdll, "RtlGetVersion"));
  if (!rtl_get_version) {
    return 0;
  }
  OSVERSIONINFOW info{};
  info.dwOSVersionInfoSize = sizeof(info);
  if (rtl_get_version(&info) != 0) {
    return 0;
  }
  return info.dwBuildNumber;
}

}  // namespace

WinMLEpBootstrapper::WinMLEpBootstrapper(std::string name, EpRegistrationCallback register_ep,
                                         std::shared_ptr<void> catalog_ref, WinMLEpHandle ep_handle)
    : name_(std::move(name)),
      register_ep_(std::move(register_ep)),
      catalog_ref_(std::move(catalog_ref)),
      ep_handle_(ep_handle) {}

const std::string& WinMLEpBootstrapper::Name() const {
  return name_;
}

bool WinMLEpBootstrapper::IsRegistered() const {
  return registered_;
}

bool WinMLEpBootstrapper::DownloadAndRegister(bool force,
                                              const ProgressCallback& progress_cb,
                                              ILogger& logger) {
  if (registered_ && !force) {
    if (progress_cb) {
      progress_cb(name_, 100.0f);
    }
    return true;
  }

  // Ask the OS to download/prepare the EP if needed.
  HRESULT hr = WinMLEpEnsureReady(ep_handle_);

  if (FAILED(hr)) {
    logger.Log(LogLevel::Warning,
               fmt::format("WinML EP {}: EnsureReady failed (hr=0x{:08X})", name_, static_cast<unsigned>(hr)));
    return false;
  }

  // Retrieve the library path from the EP handle.
  size_t path_size = 0;
  hr = WinMLEpGetLibraryPathSize(ep_handle_, &path_size);

  if (FAILED(hr) || path_size == 0) {
    logger.Log(LogLevel::Warning, fmt::format("WinML EP {}: failed to get library path size (hr=0x{:08X})", name_,
                                              static_cast<unsigned>(hr)));
    return false;
  }

  std::string path(path_size, '\0');
  hr = WinMLEpGetLibraryPath(ep_handle_, path.size(), path.data(), nullptr);

  if (FAILED(hr)) {
    logger.Log(LogLevel::Warning, fmt::format("WinML EP {}: failed to get library path (hr=0x{:08X})", name_,
                                              static_cast<unsigned>(hr)));
    return false;
  }

  // The API may include a trailing null in the reported size.
  if (!path.empty() && path.back() == '\0') {
    path.pop_back();
  }

  library_path_ = path;

  // Register with ORT via the callback.
  if (!register_ep_(name_, std::filesystem::path(path))) {
    logger.Log(LogLevel::Warning, fmt::format("WinML EP {}: ORT registration failed", name_));
    return false;
  }

  registered_ = true;

  if (progress_cb) {
    progress_cb(name_, 100.0f);
  }

  // Library path + version are logged by the central register_ep callback;
  // no extra bootstrapper-side line needed.
  return true;
}

std::vector<std::unique_ptr<WinMLEpBootstrapper>> WinMLEpBootstrapper::DiscoverProviders(
    EpRegistrationCallback register_ep,
    ILogger& logger) {
  // Pre-check that the WinML DLL is loadable. The DLL is delay-loaded, so
  // calling WinML functions without it present would cause a structured
  // exception. Loading it explicitly is cleaner than SEH.
  HMODULE winml_dll = LoadLibraryW(L"Microsoft.Windows.AI.MachineLearning.dll");

  if (!winml_dll) {
    // Diagnostic only — older Windows (< build 18362) ships without
    // Microsoft.Windows.AI.MachineLearning.dll, so EP discovery is expected
    // to fail there. Include GetLastError() and the OS build to give the
    // user a clear hint instead of an opaque "DLL not available".
    DWORD load_err = ::GetLastError();
    DWORD os_build = QueryWindowsBuild();
    logger.Log(LogLevel::Information,
               fmt::format("WinML EP catalog: DLL not available — EP discovery disabled "
                           "(LoadLibrary err={}, Windows build {})",
                           load_err, os_build));
    return {};
  }
  // Keep the DLL loaded — the delay-load stubs will resolve against it.

  WinMLEpCatalogHandle catalog = nullptr;
  HRESULT hr = WinMLEpCatalogCreate(&catalog);

  if (FAILED(hr) || catalog == nullptr) {
    logger.Log(LogLevel::Information,
               fmt::format("WinML EP catalog: creation failed (hr=0x{:08X})", static_cast<unsigned>(hr)));
    return {};
  }

  // Wrap the catalog in a shared_ptr so all bootstrappers keep it alive.
  // The last bootstrapper destroyed will release the catalog.
  auto catalog_guard = std::shared_ptr<void>(catalog, [](void* c) {
    WinMLEpCatalogRelease(static_cast<WinMLEpCatalogHandle>(c));
  });

  // Context for the enumeration callback — collects discovered bootstrappers.
  struct EnumContext {
    std::vector<std::unique_ptr<WinMLEpBootstrapper>> bootstrappers;
    std::shared_ptr<void> catalog_ref;
    EpRegistrationCallback register_ep;
    ILogger* logger;
  };

  EnumContext ctx;
  ctx.catalog_ref = catalog_guard;
  ctx.register_ep = register_ep;
  ctx.logger = &logger;

  WinMLEpCatalogEnumProviders(
      catalog,
      [](WinMLEpHandle ep, const WinMLEpInfo* info, void* context) -> BOOL {
        auto* ctx = static_cast<EnumContext*>(context);

        std::string provider_name = info->name ? info->name : "";

        ctx->logger->Log(
            LogLevel::Information,
            fmt::format("WinML EP discovered: {} (state: {})", provider_name,
                        static_cast<int>(info->readyState)));

        // If the EP is already ready, capture its library path now.
        std::string library_path;
        if (info->readyState == WinMLEpReadyState_Ready && info->libraryPath) {
          library_path = info->libraryPath;
        }

        auto bootstrapper = std::unique_ptr<WinMLEpBootstrapper>(
            new WinMLEpBootstrapper(std::move(provider_name), ctx->register_ep,
                                    ctx->catalog_ref, ep));

        if (!library_path.empty()) {
          bootstrapper->library_path_ = std::move(library_path);
        }

        ctx->bootstrappers.push_back(std::move(bootstrapper));

        return TRUE;  // continue enumeration
      },
      &ctx);

  logger.Log(LogLevel::Information,
             fmt::format("WinML EP catalog: discovered {} provider(s)", ctx.bootstrappers.size()));

  return std::move(ctx.bootstrappers);
}

}  // namespace fl
