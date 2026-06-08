// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Optional Windows App SDK bootstrap helper for non-packaged consumer processes (e.g. the
// JavaScript binding loaded into Node, where the host process has no built-in WinAppSDK
// activation). When opted in via additional_options["Bootstrap"]="true", initializes the
// Windows App Runtime framework package so that APIs depending on it — notably the WinML EP
// catalog DLL `Microsoft.Windows.AI.MachineLearning.dll` consumed by `WinMLEpBootstrapper` —
// can resolve at runtime. Defaults to off; matches the C# FoundryLocalCore behavior.
//
// Compiled only in WinML builds (FOUNDRY_LOCAL_USE_WINML=ON), which already take a hard
// dependency on the WindowsAppSDK NuGet. Outside that configuration the header is empty and
// callers must guard their use sites with the same FOUNDRY_LOCAL_USE_WINML macro.
#pragma once

#if defined(FOUNDRY_LOCAL_USE_WINML) && FOUNDRY_LOCAL_USE_WINML

namespace fl {

class ILogger;

/// Initialize the Windows App Runtime framework package for this process by calling
/// MddBootstrapInitialize2 with WinAppSDK 1.8 minimum. Idempotent — subsequent calls are
/// no-ops once initialized.
///
/// Returns true if bootstrap succeeded (or was already initialized). Returns false on
/// failure; the reason is logged but the process continues — WinML EP discovery will simply
/// find no providers.
bool TryInitializeWindowsAppSdk(ILogger& logger);

/// Reverse the effects of TryInitializeWindowsAppSdk(). Safe to call even if init never
/// succeeded. After this call the WinAppSDK framework package should not be used.
void ShutdownWindowsAppSdk(ILogger& logger);

}  // namespace fl

#endif  // FOUNDRY_LOCAL_USE_WINML
