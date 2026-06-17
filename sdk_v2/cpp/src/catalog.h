// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "model.h"

#include <string>
#include <vector>

namespace fl {

/// One page of model variants returned by `ICatalog::GetModelVersions`.
/// `next_continuation_token` is empty when there are no more pages; otherwise
/// callers pass it back to retrieve the next page.
struct ModelVersionsPage {
  std::vector<Model*> models;
  std::string next_continuation_token;
};

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

  /// Lists all known versions of a model (by alias), optionally filtered to a
  /// specific variant name. Bypasses the "latest only" filter the regular
  /// catalog refresh applies — new versions discovered by this call are
  /// integrated into the catalog's storage so the returned pointers remain
  /// valid for the lifetime of the catalog.
  ///
  /// `model_alias` is the alias of the model (e.g. "phi-4-mini"). When empty,
  /// implementations may return all versioned models from the underlying
  /// source (still subject to device/EP filtering).
  /// `variant_name` optionally narrows results to a specific variant (e.g.
  /// "Phi-4-generic-gpu"). Pass an empty string to return every variant.
  /// `max_versions` caps the number of variants returned; 0 or negative means
  /// no cap.
  /// `continuation_token` is an opaque cursor returned by a previous call to
  /// resume pagination from the underlying source. Empty starts from the
  /// beginning. Implementations that do not paginate ignore it.
  ///
  /// Maps to C# `IModelCatalog.GetModelVersionsAsync`.
  virtual ModelVersionsPage GetModelVersions(const std::string& model_alias,
                                             const std::string& variant_name,
                                             int max_versions = 0,
                                             const std::string& continuation_token = {}) = 0;

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
