// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "winml_bootstrap.h"

// Entire translation unit is empty outside Windows builds.
#ifdef _WIN32

#include "logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <atomic>
#include <cstdio>
#include <string>

namespace {

// MddBootstrap types replicated from MddBootstrap.h to avoid a hard link dep on
// Microsoft.WindowsAppRuntime.Bootstrap.lib. Loaded dynamically at runtime so
// foundry_local.dll is usable on systems without the Windows App SDK installed.
struct MddPackageVersion {
  union {
    UINT64 Version;
    struct {
      UINT16 Revision;
      UINT16 Build;
      UINT16 Minor;
      UINT16 Major;
    };
  };
};

// MddBootstrapInitializeOptions flags (subset from MddBootstrap.h)
enum MddBootstrapInitializeOptions : UINT32 {
  MddBootstrapInitializeOptions_None = 0,
};

using MddBootstrapInitialize2Fn = HRESULT(WINAPI*)(UINT32, PCWSTR, MddPackageVersion,
                                                   MddBootstrapInitializeOptions);
using MddBootstrapShutdownFn = void(WINAPI*)();

// Windows App SDK 1.8 — matches the C# FoundryLocalCore reference.
constexpr UINT32 kMajorMinorVersion = 0x00010008;

std::atomic<bool> g_initialized{false};

}  // namespace

namespace fl {

bool TryInitializeWindowsAppSdk(ILogger& logger) {
  if (g_initialized.load(std::memory_order_acquire)) {
    return true;
  }

  HMODULE bootstrap_dll = LoadLibraryW(L"Microsoft.WindowsAppRuntime.Bootstrap.dll");
  if (!bootstrap_dll) {
    logger.Log(LogLevel::Information,
               "WindowsAppSdk bootstrap: Microsoft.WindowsAppRuntime.Bootstrap.dll not found — "
               "WinML EP bootstrap skipped.");
    return false;
  }

  auto* init_fn = reinterpret_cast<MddBootstrapInitialize2Fn>(
      GetProcAddress(bootstrap_dll, "MddBootstrapInitialize2"));
  if (!init_fn) {
    logger.Log(LogLevel::Warning,
               "WindowsAppSdk bootstrap: MddBootstrapInitialize2 not found in Bootstrap DLL.");
    FreeLibrary(bootstrap_dll);
    return false;
  }

  MddPackageVersion min_version{};
  min_version.Major = 1;
  min_version.Minor = 8;
  min_version.Build = 1;
  min_version.Revision = 0;

  HRESULT hr = init_fn(kMajorMinorVersion, nullptr, min_version, MddBootstrapInitializeOptions_None);
  if (FAILED(hr)) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%08lX", static_cast<unsigned long>(hr));
    logger.Log(LogLevel::Information,
               std::string("WindowsAppSdk bootstrap: MddBootstrapInitialize2 failed (HRESULT=0x") +
                   buf + "). WinML EPs will not be available.");
    // Don't FreeLibrary — leak is intentional to keep the module handle stable
    // if a subsequent call succeeds after a transient failure.
    return false;
  }

  // Keep the DLL loaded — MddBootstrapShutdown needs it.
  g_initialized.store(true, std::memory_order_release);
  logger.Log(LogLevel::Information, "WindowsAppSdk bootstrap: initialized successfully (WinAppSDK >= 1.8.1.0).");
  return true;
}

void ShutdownWindowsAppSdk(ILogger& logger) {
  if (!g_initialized.exchange(false, std::memory_order_acq_rel)) {
    return;
  }

  HMODULE bootstrap_dll = GetModuleHandleW(L"Microsoft.WindowsAppRuntime.Bootstrap.dll");
  if (bootstrap_dll) {
    auto* shutdown_fn = reinterpret_cast<MddBootstrapShutdownFn>(
        GetProcAddress(bootstrap_dll, "MddBootstrapShutdown"));
    if (shutdown_fn) {
      shutdown_fn();
    }
  }

  logger.Log(LogLevel::Information, "WindowsAppSdk bootstrap: shutdown complete.");
}

}  // namespace fl

#endif  // _WIN32
