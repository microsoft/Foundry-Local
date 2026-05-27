// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "catalog.h"
#include "logger.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace fl {

/// BaseModelCatalog: holds model collection, indices, and lookup logic.
/// Derived classes implement FetchModels() to provide models from their specific source.
/// The base class owns caching, indexing, and thread-safe refresh.
///
/// Model ownership: The catalog owns all Model instances via unique_ptr in models_.
/// These pointers are stable for the lifetime of the catalog — external code can hold
/// raw Model* pointers safely. Indices (id_index, alias_index, name_index) are rebuilt
/// on refresh but always point into the stable models_ storage.
///
/// Maps to C# BaseModelCatalog<TModelInfo>.
class BaseModelCatalog : public ICatalog {
 public:
  BaseModelCatalog(std::string name, ILogger& logger);
  ~BaseModelCatalog() override;

  const std::string& GetName() const override { return name_; }

  // ICatalog implementations — query/lookup layer
  std::vector<Model*> ListModels() const override;
  Model* GetModel(const std::string& alias) const override;
  Model* GetModelVariant(const std::string& model_id) const override;
  Model* GetLatestVersion(const Model* model) const override;
  std::vector<Model*> GetCachedModels() const override;
  std::vector<Model*> GetLoadedModels() const override;
  void InvalidateCache() override;

 protected:
  /// Derived classes implement this to fetch model variants from their source.
  /// Returns the full variant list. Base class handles caching and indexing.
  /// Maps to C# FetchModelInfoAsync.
  virtual std::vector<Model> FetchModels() const = 0;

 private:
  /// Lookup indices into the stable models_ storage.
  /// Rebuilt on refresh. Does not own any Model instances.
  struct ModelIndex {
    std::unordered_map<std::string, Model*> id_index;     // model_id -> Model* (specific variant)
    std::unordered_map<std::string, Model*> alias_index;  // alias -> Model* (grouped container)
    std::unordered_map<std::string, Model*> name_index;   // name -> latest version Model*
  };

  /// Stable model storage. unique_ptr ensures addresses never change.
  /// Models are only appended, never removed — external Model* pointers remain valid.
  mutable std::vector<std::unique_ptr<Model>> models_;

  /// Lookup indices, rebuilt on each populate/refresh.
  /// Guarded by std::atomic_load/store free functions so readers get a consistent
  /// snapshot — the swap after rebuild is atomic, so a concurrent reader never sees
  /// a partially-built index.
  /// Can't use std::atomic<std::shared_ptr<>> due to lack of implemention in XCode (macOS)
  mutable std::shared_ptr<const ModelIndex> index_;

  /// Atomically grab the current index snapshot. Callers hold the returned shared_ptr
  /// for the duration of their lookup, keeping the index alive if a refresh swaps it out.
  std::shared_ptr<const ModelIndex> GetIndex() const;

  mutable bool populated_ = false;
  mutable std::mutex mutex_;
  mutable std::chrono::steady_clock::time_point next_refresh_at_{};

  static constexpr std::chrono::hours kCacheDuration{4};

  /// Populate or refresh the catalog (under lock). Groups variants, builds indices.
  void PopulateModels(std::vector<Model> variants) const;

  /// Build lookup indices from the current models_ collection.
  /// Builds a complete new ModelIndex locally, then atomically swaps it into index_.
  void RebuildIndex() const;

  /// Thread-safe access: ensures catalog is populated, refreshes if allowed and stale.
  void EnsurePopulated(bool allow_refresh = false) const;

  std::string name_;
  ILogger& logger_;
};

}  // namespace fl
