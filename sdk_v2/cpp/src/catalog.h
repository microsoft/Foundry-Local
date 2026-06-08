// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "model.h"

#include <string>
#include <vector>

namespace fl {

/// Abstract catalog interface for querying available models.
/// Mirrors the C API's flCatalogApi surface.
class ICatalog {
 public:
  virtual ~ICatalog() = default;

  /// Returns a human-readable name for this catalog.
  /// For Azure catalogs this is the catalog URI.
  virtual const std::string& GetName() const = 0;

  /// Lists all models in the catalog.
  virtual std::vector<Model*> ListModels() const = 0;

  /// Gets a model by alias. Returns nullptr if not found.
  virtual Model* GetModel(const std::string& alias) const = 0;

  /// Gets a specific model variant by model_id. Returns nullptr if not found.
  virtual Model* GetModelVariant(const std::string& model_id) const = 0;

  /// Gets the latest version of a model. Returns nullptr if not found.
  virtual Model* GetLatestVersion(const Model* model) const = 0;

  /// Lists only models that are cached locally.
  virtual std::vector<Model*> GetCachedModels() const = 0;

  /// Lists only models that are currently loaded into a runtime.
  virtual std::vector<Model*> GetLoadedModels() const = 0;

  /// Invalidate the cached model list so the next query re-fetches.
  /// Called after EP registration changes, since the available device filters
  /// may now include additional execution providers.
  virtual void InvalidateCache() {}
};

}  // namespace fl
