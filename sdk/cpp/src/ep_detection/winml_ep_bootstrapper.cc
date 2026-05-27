// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "ep_detection/winml_ep_bootstrapper.h"

#include "logger.h"

#include <fmt/format.h>

#include <string>
#include <vector>

#ifdef _WIN32

// WinML EP Catalog C API — delay-loaded via Microsoft.Windows.AI.MachineLearning.dll
#if FOUNDRY_LOCAL_HAS_EP_CATALOG
#include <WinMLEpCatalog.h>
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {

/// Checks for Windows 10 build 26100+ (Windows 11 24H2), the minimum OS
/// version that ships the WinML EP catalog.
///
/// Uses RtlGetVersion (ntdll) because VerifyVersionInfoW lies without a
/// compatibility manifest — it caps the reported version at 6.2 (Win 8.1)
/// unless the app declares support for newer Windows versions.
bool IsWindows11_24H2OrLater() {
  // RtlGetVersion is always available and always returns the true OS version.
  using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
  auto* ntdll = GetModuleHandleW(L"ntdll.dll");
  if (!ntdll) {
    return false;
  }

  auto rtl_get_version = reinterpret_cast<RtlGetVersionFn>(
      GetProcAddress(ntdll, "RtlGetVersion"));
  if (!rtl_get_version) {
    return false;
  }

  OSVERSIONINFOW osvi = {};
  osvi.dwOSVersionInfoSize = sizeof(osvi);
  if (rtl_get_version(&osvi) != 0) {
    return false;
  }

  // Windows 11 24H2 = build 26100+
  if (osvi.dwMajorVersion > 10) {
    return true;
  }
  if (osvi.dwMajorVersion == 10 && osvi.dwMinorVersion == 0) {
    return osvi.dwBuildNumber >= 26100;
  }

  return false;
}

}  // anonymous namespace

namespace fl {

#if FOUNDRY_LOCAL_HAS_EP_CATALOG
WinMLEpBootstrapper::WinMLEpBootstrapper(std::string name, EpRegistrationCallback register_ep,
                                         std::shared_ptr<void> catalog_ref, WinMLEpHandle ep_handle)
    : name_(std::move(name)),
      register_ep_(std::move(register_ep)),
      catalog_ref_(std::move(catalog_ref)),
      ep_handle_(ep_handle) {}
#endif

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

#if !FOUNDRY_LOCAL_HAS_EP_CATALOG
  logger.Log(LogLevel::Warning,
             fmt::format("WinML EP {}: EP catalog not available at compile time", name_));
  return false;
#else
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
#endif
}

std::vector<std::unique_ptr<WinMLEpBootstrapper>> WinMLEpBootstrapper::DiscoverProviders(
    EpRegistrationCallback register_ep,
    ILogger& logger) {
#if !FOUNDRY_LOCAL_HAS_EP_CATALOG
  (void)register_ep;
  (void)logger;
  return {};
#else
  if (!IsWindows11_24H2OrLater()) {
    logger.Log(LogLevel::Information,
               "WinML EP catalog: requires Windows 11 24H2+ (build 26100)");
    return {};
  }

  // Pre-check that the WinML DLL is loadable. The DLL is delay-loaded, so
  // calling WinML functions without it present would cause a structured
  // exception. Loading it explicitly is cleaner than SEH.
  HMODULE winml_dll = LoadLibraryW(L"Microsoft.Windows.AI.MachineLearning.dll");

  if (!winml_dll) {
    logger.Log(LogLevel::Information,
               "WinML EP catalog: DLL not available — EP discovery disabled");
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
#endif
}

}  // namespace fl

#endif  // _WIN32
