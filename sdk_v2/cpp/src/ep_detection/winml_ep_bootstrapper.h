// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#ifdef _WIN32

#include "ep_detection/ep_bootstrapper.h"
#include "ep_detection/ep_types.h"

#if FOUNDRY_LOCAL_HAS_EP_CATALOG
#include <WinMLEpCatalog.h>
#endif

#include <memory>
#include <string>
#include <vector>

namespace fl {

class ILogger;

/// Bootstrapper for a single WinML-based execution provider.
/// Each instance wraps one WinMLEpHandle from the WinML EP catalog.
/// Windows 11 24H2+ (build 26100) only.
class WinMLEpBootstrapper : public IEpBootstrapper {
 public:
  ~WinMLEpBootstrapper() override = default;

  // Non-copyable
  WinMLEpBootstrapper(const WinMLEpBootstrapper&) = delete;
  WinMLEpBootstrapper& operator=(const WinMLEpBootstrapper&) = delete;

  // Movable (for vector storage)
  WinMLEpBootstrapper(WinMLEpBootstrapper&&) noexcept = default;
  WinMLEpBootstrapper& operator=(WinMLEpBootstrapper&&) noexcept = default;

  const std::string& Name() const override;
  bool IsRegistered() const override;
  bool DownloadAndRegister(bool force,
                           const ProgressCallback& progress_cb,
                           ILogger& logger) override;

  /// Discovers all WinML EPs available on this system.
  /// Returns empty on non-Windows, unsupported OS version, or missing WinML DLL.
  /// @param register_ep  Callback called after EnsureReady to register the EP with ORT.
  /// @param logger  Logger for diagnostic output.
  static std::vector<std::unique_ptr<WinMLEpBootstrapper>> DiscoverProviders(
      EpRegistrationCallback register_ep,
      ILogger& logger);

 private:
  std::string name_;
  std::string library_path_;
  bool registered_ = false;
  EpRegistrationCallback register_ep_;

#if FOUNDRY_LOCAL_HAS_EP_CATALOG
  // Shared across all bootstrappers from the same DiscoverProviders() call
  // to keep the catalog alive until the last bootstrapper is destroyed.
  std::shared_ptr<void> catalog_ref_;
  WinMLEpHandle ep_handle_ = nullptr;

  WinMLEpBootstrapper(std::string name, EpRegistrationCallback register_ep,
                      std::shared_ptr<void> catalog_ref, WinMLEpHandle ep_handle);
#endif
};

}  // namespace fl

#endif  // _WIN32
