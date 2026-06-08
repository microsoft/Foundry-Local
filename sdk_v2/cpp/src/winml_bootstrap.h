// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Windows App SDK bootstrap helper for non-packaged consumer processes (e.g. the
// JavaScript binding loaded into Node, where the host process has no built-in WinAppSDK
// activation). When opted in via additional_options["Bootstrap"]="true", initializes the
// Windows App Runtime framework package so that APIs depending on it — notably the WinML EP
// catalog DLL `Microsoft.Windows.AI.MachineLearning.dll` consumed by `WinMLEpBootstrapper` —
// can resolve at runtime. Defaults to off; the C# and JS bindings set it automatically.
//
// The bootstrap DLL (Microsoft.WindowsAppRuntime.Bootstrap.dll) is loaded dynamically, so
// foundry_local.dll is usable on machines without the Windows App SDK installed — failure is
// silent and WinML EP discovery simply finds no providers.
#pragma once

#ifdef _WIN32

namespace fl {

class ILogger;

/// Initialize the Windows App Runtime framework package for this process by calling
/// MddBootstrapInitialize2 with WinAppSDK 1.8 minimum. Idempotent — subsequent calls are
/// no-ops once initialized.
///
/// Returns true if bootstrap succeeded (or was already initialized). Returns false on
/// failure (e.g. WinAppSDK not installed); the reason is logged at Information level.
bool TryInitializeWindowsAppSdk(ILogger& logger);

/// Reverse the effects of TryInitializeWindowsAppSdk(). Safe to call even if init never
/// succeeded. After this call the WinAppSDK framework package should not be used.
void ShutdownWindowsAppSdk(ILogger& logger);

}  // namespace fl

#endif  // _WIN32
