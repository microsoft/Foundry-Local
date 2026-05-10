// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "model.h"

#include "download/download_manager.h"
#include "exception.h"
#include "inferencing/model_load_manager.h"
#include "items/item.h"
#include "items/text_item.h"
#include "utils.h"

#include <foundry_local/foundry_local_c.h>
#include <nlohmann/json.hpp>

namespace fl {

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

Model::~Model() = default;

Model::Model(Model&& other) noexcept
    : info_(std::move(other.info_)),
      loaded_(other.loaded_),
      cached_(other.cached_),
      local_path_(std::move(other.local_path_)),
      download_manager_(other.download_manager_),
      model_load_manager_(other.model_load_manager_),
      variants_(std::move(other.variants_)),
      selected_variant_(other.selected_variant_) {
  // After vector move, selected_variant_ still points into the transferred buffer.
  other.download_manager_ = nullptr;
  other.model_load_manager_ = nullptr;
  other.selected_variant_ = nullptr;
  // variants_cache_ intentionally left empty — rebuilt lazily under lock.
}

Model& Model::operator=(Model&& other) noexcept {
  if (this != &other) {
    info_ = std::move(other.info_);
    loaded_ = other.loaded_;
    cached_ = other.cached_;
    local_path_ = std::move(other.local_path_);
    download_manager_ = other.download_manager_;
    model_load_manager_ = other.model_load_manager_;
    variants_ = std::move(other.variants_);
    selected_variant_ = other.selected_variant_;
    other.download_manager_ = nullptr;
    other.model_load_manager_ = nullptr;
    other.selected_variant_ = nullptr;
    variants_cache_.clear();
  }

  return *this;
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

Model Model::FromModelInfo(ModelInfo info,
                           std::string local_path,
                           DownloadManager* download_manager,
                           ModelLoadManager* model_load_manager) {
  Model model;
  model.info_ = std::move(info);
  model.download_manager_ = download_manager;
  model.model_load_manager_ = model_load_manager;

  if (!local_path.empty()) {
    model.cached_ = true;
    model.local_path_ = std::move(local_path);
  }

  return model;
}

// ---------------------------------------------------------------------------
// Container operations
// ---------------------------------------------------------------------------

Model Model::MakeContainer(Model first_variant) {
  Model container;
  container.variants_.push_back(std::move(first_variant));
  container.selected_variant_ = &container.variants_.back();
  return container;
}

void Model::AddVariant(Model variant) {
  if (!IsContainer()) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL, "AddVariant called on a non-container Model; use MakeContainer first");
  }

  // Prefer a cached variant over a non-cached selection (matches C# behavior).
  bool prefer_new = variant.IsCached() && !selected_variant_->IsCached();

  // Save selected variant's index before push_back (which may reallocate the vector,
  // invalidating the old selected_variant_ pointer).
  size_t selected_idx = static_cast<size_t>(selected_variant_ - variants_.data());

  variants_.push_back(std::move(variant));

  if (prefer_new) {
    selected_variant_ = &variants_.back();
  } else {
    selected_variant_ = &variants_[selected_idx];  // Restore after potential reallocation
  }

  variants_cache_.clear();  // invalidate; rebuilt lazily under lock
}

// ---------------------------------------------------------------------------
// Properties (delegate to selected_variant_ when container)
// ---------------------------------------------------------------------------

const std::string& Model::Id() const {
  if (selected_variant_) {
    return selected_variant_->Id();
  }

  return info_.model_id;
}

const std::string& Model::Alias() const {
  if (selected_variant_) {
    return selected_variant_->Alias();
  }

  return info_.alias;
}

const ModelInfo& Model::Info() const {
  if (selected_variant_) {
    return selected_variant_->Info();
  }

  return info_;
}

const std::vector<Model*>& Model::Variants() const {
  std::lock_guard<std::mutex> lock(variants_cache_mutex_);

  if (variants_cache_.size() != variants_.size()) {
    RebuildVariantsCache();
  }

  // Leaf: return {this}
  if (!IsContainer() && variants_cache_.empty()) {
    variants_cache_.push_back(const_cast<Model*>(this));
  }

  return variants_cache_;
}

bool Model::IsCached() const {
  if (selected_variant_) {
    return selected_variant_->IsCached();
  }

  return cached_;
}

bool Model::IsLoaded() const {
  if (selected_variant_) {
    return selected_variant_->IsLoaded();
  }

  return loaded_;
}

// ---------------------------------------------------------------------------
// Mutation (delegate to selected_variant_ when container)
// ---------------------------------------------------------------------------

void Model::Download(std::function<void(float)> progress_cb) {
  if (selected_variant_) {
    selected_variant_->Download(std::move(progress_cb));
    return;
  }

  // Already cached (scanner found the model on disk during catalog construction).
  // No need to re-derive the path via DownloadManager — local_path_ is authoritative.
  if (cached_ && !local_path_.empty()) {
    if (progress_cb) {
      progress_cb(100.0f);
    }
    return;
  }

  if (download_manager_ == nullptr) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,  // we control setting these so internal error
             "model is not attached to a DownloadManager");
  }

  auto path = download_manager_->DownloadModel(info_, std::move(progress_cb));
  cached_ = true;
  local_path_ = std::move(path);
}

const std::string& Model::GetPath() const {
  if (selected_variant_) {
    return selected_variant_->GetPath();
  }

  return local_path_;
}

void Model::Load(ExecutionProvider ep) {
  if (selected_variant_) {
    selected_variant_->Load(ep);
    return;
  }

  if (model_load_manager_ == nullptr) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,  // we control setting these so internal error
             "model is not attached to a ModelLoadManager");
  }

  Load(*model_load_manager_, ep);
}

void Model::Load(ModelLoadManager& load_manager, ExecutionProvider ep) {
  if (selected_variant_) {
    selected_variant_->Load(load_manager, ep);
    return;
  }

  model_load_manager_ = &load_manager;

  if (loaded_) {
    return;  // Already loaded
  }

  auto result = load_manager.LoadModel(local_path_, info_.model_id, ep);

  if (result.status == ModelLoadManager::LoadStatus::kModelNotFound) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL, "model not found at path: " + local_path_);
  }

  loaded_ = true;
}

void Model::Unload() {
  if (selected_variant_) {
    selected_variant_->Unload();
    return;
  }

  if (model_load_manager_ == nullptr) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL,  // we control setting these so internal error
             "model is not attached to a ModelLoadManager");
  }

  Unload(*model_load_manager_);
}

void Model::Unload(ModelLoadManager& load_manager) {
  if (selected_variant_) {
    selected_variant_->Unload(load_manager);
    return;
  }

  model_load_manager_ = &load_manager;

  if (!loaded_) {
    return;  // Not loaded
  }

  load_manager.UnloadModel(info_.model_id);
  loaded_ = false;
}

void Model::RemoveFromCache() {
  if (selected_variant_) {
    selected_variant_->RemoveFromCache();
    return;
  }

  if (!cached_ || local_path_.empty()) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_USAGE, "model is not cached locally");
  }

  if (loaded_) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_USAGE, "cannot remove a loaded model from cache; unload it first");
  }

  if (!Utils::RemoveDirectoryRecursive(local_path_)) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL, "failed to remove model cache directory: " + local_path_);
  }

  cached_ = false;
  local_path_.clear();
}

void Model::SelectVariant(Model& variant) {
  if (!IsContainer()) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT,
             "Not supported on a model variant. Fetch a model by alias from the catalog to get a model "
             "with all variants available.");
  }

  for (auto& v : variants_) {
    if (&v == &variant) {
      selected_variant_ = &v;
      return;
    }
  }

  FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT, "variant not found in this model");
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

void Model::RebuildVariantsCache() const {
  variants_cache_.clear();
  variants_cache_.reserve(variants_.size());

  for (auto& v : variants_) {
    variants_cache_.push_back(const_cast<Model*>(&v));
  }
}

// Static IO type descriptors, shared across all Model instances with the same task.
// Built once per task on first access (C++11 guarantees thread-safe function-local static init).
namespace {

struct StaticIOCache {
  std::vector<std::unique_ptr<Item>> input_items;
  std::vector<std::unique_ptr<Item>> output_items;
  std::vector<const Item*> input_ptrs;
  std::vector<const Item*> output_ptrs;
};

const StaticIOCache& ChatCompletionIO() {
  static const StaticIOCache cache = [] {
    StaticIOCache c;
    c.input_items.push_back(Item::Create(FOUNDRY_LOCAL_ITEM_MESSAGE));
    c.input_items.push_back(std::make_unique<TextItem>("", FOUNDRY_LOCAL_TEXT_ITEM_TYPE_OPENAI_JSON));
    c.output_items.push_back(Item::Create(FOUNDRY_LOCAL_ITEM_MESSAGE));
    c.output_items.push_back(std::make_unique<TextItem>("", FOUNDRY_LOCAL_TEXT_ITEM_TYPE_OPENAI_JSON));

    for (const auto& item : c.input_items) {
      c.input_ptrs.push_back(item.get());
    }

    for (const auto& item : c.output_items) {
      c.output_ptrs.push_back(item.get());
    }

    return c;
  }();

  return cache;
}

const StaticIOCache& AutomaticSpeechRecognitionIO() {
  static const StaticIOCache cache = [] {
    StaticIOCache c;
    c.input_items.push_back(Item::Create(FOUNDRY_LOCAL_ITEM_AUDIO));
    c.input_items.push_back(std::make_unique<TextItem>("", FOUNDRY_LOCAL_TEXT_ITEM_TYPE_OPENAI_JSON));
    c.output_items.push_back(Item::Create(FOUNDRY_LOCAL_ITEM_TEXT));
    c.output_items.push_back(std::make_unique<TextItem>("", FOUNDRY_LOCAL_TEXT_ITEM_TYPE_OPENAI_JSON));

    for (const auto& item : c.input_items) {
      c.input_ptrs.push_back(item.get());
    }

    for (const auto& item : c.output_items) {
      c.output_ptrs.push_back(item.get());
    }

    return c;
  }();

  return cache;
}

Model::IOInfo IOInfoFromCache(const StaticIOCache& cache) {
  return {cache.input_ptrs.data(), cache.input_ptrs.size(),
          cache.output_ptrs.data(), cache.output_ptrs.size()};
}

}  // namespace

Model::IOInfo Model::GetInputOutputInfo() const {
  if (selected_variant_) {
    return selected_variant_->GetInputOutputInfo();
  }

  const auto& task = Info().task;

  if (task == "chat-completion") {
    return IOInfoFromCache(ChatCompletionIO());
  }

  if (task == "automatic-speech-recognition") {
    return IOInfoFromCache(AutomaticSpeechRecognitionIO());
  }

  FL_THROW(FOUNDRY_LOCAL_ERROR_NOT_IMPLEMENTED,
           "input/output info not defined for task: " + task);
}

}  // namespace fl
