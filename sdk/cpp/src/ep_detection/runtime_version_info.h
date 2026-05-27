// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <string>

// Forward declarations — avoid pulling onnxruntime_c_api.h into this header.
struct OrtApi;
struct OrtEnv;

namespace fl {

class ILogger;

/// Returns the version string ORT publishes for a registered EP, looked up via
/// OrtApi::GetEpDevices + EpMetadata["version"]. Returns "unknown" if the EP
/// isn't registered or doesn't expose a version.
std::string GetEpVersion(const OrtApi& ort_api, OrtEnv& ort_env, const std::string& ep_name);

/// Logs the onnxruntime runtime version. ORT-GenAI does not expose a runtime
/// version query, so it is omitted; the SDK build version is captured by
/// foundry_local::Version() on the public API surface.
void LogRuntimeVersions(ILogger& logger);

}  // namespace fl
