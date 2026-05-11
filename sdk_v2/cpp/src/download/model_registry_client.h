// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <functional>
#include <string>

namespace fl {

/// HTTP GET function signature for dependency injection / testing.
using HttpGetFn = std::function<std::string(const std::string& url)>;

/// Result from resolving a model's asset ID against the Azure model registry.
struct ModelContainer {
  std::string blob_sas_uri;
  std::string description;
};

/// Client for the Azure model registry API.
/// Resolves a model's asset_id to a Blob Storage SAS URI for downloading.
class ModelRegistryClient {
 public:
  /// @param region Azure region for the model registry endpoint (e.g. "eastus").
  ///               Used to construct https://{region}.api.azureml.ms/modelregistry/...
  explicit ModelRegistryClient(std::string region = "eastus");

  /// Override the HTTP GET function (for testing).
  void SetHttpGet(HttpGetFn fn);

  /// Resolve a model's asset_id (URI) to a blob storage SAS URI.
  /// Uses anonymous access (no token) for FoundryLocal models.
  /// Throws fl::Exception on failure.
  ModelContainer ResolveModelContainer(const std::string& asset_id);

 private:
  std::string base_url_;
  HttpGetFn http_get_;
};

}  // namespace fl
