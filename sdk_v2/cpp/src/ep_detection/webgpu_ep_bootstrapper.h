// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "ep_detection/ep_bootstrapper.h"
#include "ep_detection/ep_types.h"

#include <string>
#include <unordered_map>

namespace fl {

class ILogger;

/// Bootstrapper for the WebGPU execution provider.
///
/// Fetches a manifest from Azure CDN to discover the current WebGPU EP
/// package URL and expected SHA-256 hashes, downloads the binaries, verifies
/// integrity, then registers with ORT.  The manifest-driven approach allows
/// updating WebGPU EP binaries without shipping a new Foundry Local release.
///
/// Supports Windows x64/ARM64, Linux x64, and macOS ARM64.
class WebGpuEpBootstrapper : public IEpBootstrapper {
 public:
  /// @param ep_dir  Base directory for EP packages (e.g., appdata/foundry-local).
  ///                The WebGPU package will be at ep_dir/webgpu-ep/.
  /// @param register_ep  Callback to register the EP DLL with ORT.
  WebGpuEpBootstrapper(std::string ep_dir, EpRegistrationCallback register_ep);
  ~WebGpuEpBootstrapper() override = default;

  // Non-copyable
  WebGpuEpBootstrapper(const WebGpuEpBootstrapper&) = delete;
  WebGpuEpBootstrapper& operator=(const WebGpuEpBootstrapper&) = delete;

  const std::string& Name() const override;
  bool IsRegistered() const override;
  bool DownloadAndRegister(bool force,
                           const ProgressCallback& progress_cb,
                           ILogger& logger) override;

 private:
  /// Verify all expected binaries exist in @p dir with correct SHA-256 hashes.
  static bool VerifyPackage(const std::filesystem::path& dir,
                            const std::unordered_map<std::string, std::string>& expected_hashes,
                            ILogger& logger);

  std::string ep_dir_;
  std::string name_ = "WebGpuExecutionProvider";
  bool registered_ = false;
  int attempts_ = 0;
  EpRegistrationCallback register_ep_;
};

}  // namespace fl
