// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "winml_bootstrap.h"

// Entire translation unit is empty outside WinML builds — see winml_bootstrap.h. Callers
// guard their use sites with the same FOUNDRY_LOCAL_USE_WINML macro, so there are no
// undefined references to no-op stubs.
#if defined(FOUNDRY_LOCAL_USE_WINML) && FOUNDRY_LOCAL_USE_WINML

#include "logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <MddBootstrap.h>

#include <atomic>
#include <cstdio>
#include <string>

namespace {

// Windows App SDK 1.8 — matches the C# FoundryLocalCore reference (majorMinorVersion=
// 0x00010008, minVersion={1,8,1,0}). Update in lockstep with the WinML EP catalog NuGet.
constexpr UINT32 kMajorMinorVersion = 0x00010008;
constexpr UINT16 kMinMajor = 1;
constexpr UINT16 kMinMinor = 8;
constexpr UINT16 kMinBuild = 1;
constexpr UINT16 kMinRevision = 0;

std::atomic<bool> g_initialized{false};

}  // namespace

namespace fl {

bool TryInitializeWindowsAppSdk(ILogger& logger) {
  if (g_initialized.load(std::memory_order_acquire)) {
    return true;
  }

  PACKAGE_VERSION min_version{};
  min_version.Major = kMinMajor;
  min_version.Minor = kMinMinor;
  min_version.Build = kMinBuild;
  min_version.Revision = kMinRevision;

  HRESULT hr = ::MddBootstrapInitialize2(
      kMajorMinorVersion, nullptr, min_version,
      MddBootstrapInitializeOptions_OnNoMatch_ShowUI);
  if (FAILED(hr)) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%08lX", static_cast<unsigned long>(hr));
    logger.Log(LogLevel::Warning,
               std::string("WindowsAppSdk bootstrap: MddBootstrapInitialize2 failed (HRESULT=0x") +
                   buf + "). WinML EP discovery may find no providers.");
    return false;
  }

  g_initialized.store(true, std::memory_order_release);
  logger.Log(LogLevel::Information,
             "WindowsAppSdk bootstrap: initialized successfully (WinAppSDK >= 1.8.1.0).");
  return true;
}

void ShutdownWindowsAppSdk(ILogger& logger) {
  if (!g_initialized.exchange(false, std::memory_order_acq_rel)) {
    return;
  }

  ::MddBootstrapShutdown();
  logger.Log(LogLevel::Information, "WindowsAppSdk bootstrap: shutdown complete.");
}

}  // namespace fl

#endif  // FOUNDRY_LOCAL_USE_WINML
