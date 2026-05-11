// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "inferencing/execution_provider.h"
#include "model_info.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace fl {

class DownloadManager;
struct Item;
class ModelLoadManager;

// -----------------------------------------------------------------------
// Model — a single model entry or a multi-variant container.
//
// In "leaf" mode (created via FromModelInfo), it represents a single model
// variant with its own info, cached/loaded state, and local path.
//
// In "container" mode (created via MakeContainer), it groups multiple
// Model leaves that share the same alias and delegates all operations
// to a selected variant. Additional variants are added via AddVariant.
//
// The C API type (flModel) inherits from this with zero extra members,
// following the same pattern as flItem : fl::Item.
// -----------------------------------------------------------------------

class Model {
 public:
  Model() = default;
  ~Model();
  Model(Model&& other) noexcept;
  Model& operator=(Model&& other) noexcept;
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;

  /// Create a leaf Model from a ModelInfo (used when populating the catalog).
  /// If local_path is non-empty the model is marked as cached at that location.
  /// The managers are optional non-owning bindings used by Download/Load/Unload.
  /// They should always be provided in production usage from Manager::Instance.
  /// Allowing them to be nullptr simplifies testing of various areas in isolation.
  static Model FromModelInfo(ModelInfo info,
                             std::string local_path = {},
                             DownloadManager* download_manager = nullptr,
                             ModelLoadManager* model_load_manager = nullptr);

  // --- Container construction ---

  /// Create a container Model wrapping the given variant as its first (and selected) variant.
  /// The container's own leaf data stays default/empty — all property access delegates to
  /// the selected variant.
  static Model MakeContainer(Model first_variant);

  /// Add a variant to this container. Requires IsContainer() to be true.
  /// If the new variant is cached and the current selection is not, the new variant
  /// becomes the selected variant (matches C# behavior).
  void AddVariant(Model variant);

  // --- Properties ---

  /// Unique model identifier. For containers, delegates to the selected variant.
  const std::string& Id() const;

  /// Alias shared across variants. For containers, delegates to the selected variant.
  const std::string& Alias() const;

  /// Full model metadata. For containers, delegates to the selected variant.
  const ModelInfo& Info() const;

  /// Returns the variants within this model. For a leaf, returns {this}.
  /// For a container, returns pointers to all contained variants.
  const std::vector<Model*>& Variants() const;

  // --- Query methods ---

  bool IsCached() const;
  bool IsLoaded() const;

  /// Get the supported input and output item types for this model, based on its task.
  /// Returns arrays of Item pointers (type-tag-only descriptors) from static storage.
  /// Pointers are stable for the lifetime of the process.
  /// Throws if the model's task doesn't have defined input/output info.
  struct IOInfo {
    const Item* const* inputs;
    size_t num_inputs;
    const Item* const* outputs;
    size_t num_outputs;
  };

  IOInfo GetInputOutputInfo() const;

  /// True if this is a multi-variant container.
  bool IsContainer() const { return selected_variant_ != nullptr; }

  // --- Mutation methods ---

  void Download(std::function<void(float)> progress_cb = nullptr);
  const std::string& GetPath() const;
  void Load(ExecutionProvider ep = ExecutionProvider::kDefault);
  void Load(ModelLoadManager& load_manager, ExecutionProvider ep = ExecutionProvider::kDefault);
  void Unload();
  void Unload(ModelLoadManager& load_manager);
  void RemoveFromCache();

  /// Select a specific variant within this container. Throws if the variant is
  /// not part of this model, or if this is a leaf.
  void SelectVariant(Model& variant);

  // --- Non-delegating accessors ---

  /// Get the local path if cached, or empty string if not.
  const std::string& LocalPath() const { return local_path_; }

 private:
  // Leaf data (always present; default/empty for containers)
  ModelInfo info_;
  bool loaded_ = false;
  bool cached_ = false;
  std::string local_path_;

  // Non-owning service bindings for leaf operations.
  DownloadManager* download_manager_ = nullptr;
  ModelLoadManager* model_load_manager_ = nullptr;

  // Container data (empty/null for leaves)
  std::vector<Model> variants_;
  Model* selected_variant_ = nullptr;  // non-null = this is a container
  mutable std::mutex variants_cache_mutex_;
  mutable std::vector<Model*> variants_cache_;

  void RebuildVariantsCache() const;
};

}  // namespace fl
