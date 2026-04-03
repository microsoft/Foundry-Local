// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <functional>
#include <filesystem>

#include <gsl/span>

#include "foundry_local.h"
#include "foundry_local_internal_core.h"
#include "foundry_local_exception.h"
#include "core_helpers.h"
#include "logger.h"

namespace foundry_local {

    using namespace detail;

    /// ModelVariant

    ModelVariant::ModelVariant(gsl::not_null<Internal::IFoundryLocalCore*> core, ModelInfo info,
                               gsl::not_null<ILogger*> logger)
        : core_(core), info_(std::move(info)), logger_(logger) {}

    const ModelInfo& ModelVariant::GetInfo() const {
        return info_;
    }

    void ModelVariant::RemoveFromCache() {
        auto response = CallWithJson(core_, "remove_cached_model", MakeModelParams(info_.name), *logger_);
        if (response.HasError()) {
            throw Exception("Error removing model from cache [" + info_.name + "]: " + response.error, *logger_);
        }
        cachedPath_.clear();
    }

    void ModelVariant::Unload() {
        auto response = CallWithJson(core_, "unload_model", MakeModelParams(info_.name), *logger_);
        if (response.HasError()) {
            throw Exception("Error unloading model [" + info_.name + "]: " + response.error, *logger_);
        }
    }

    bool ModelVariant::IsLoaded() const {
        std::vector<std::string> loadedModelIds = GetLoadedModelsInternal(core_, *logger_);
        for (const auto& id : loadedModelIds) {
            if (id == info_.id) {
                return true;
            }
        }

        return false;
    }

    bool ModelVariant::IsCached() const {
        auto cachedModels = GetCachedModelsInternal(core_, *logger_);
        for (const auto& id : cachedModels) {
            if (id == info_.id) {
                return true;
            }
        }
        return false;
    }

    void ModelVariant::Download(DownloadProgressCallback onProgress) {
        if (IsCached()) {
            logger_->Log(LogLevel::Information, "Model '" + info_.name + "' is already cached, skipping download.");
            return;
        }

        if (onProgress) {
            struct ProgressState {
                DownloadProgressCallback* cb;
                ILogger* logger;
            } state{&onProgress, logger_};

            auto nativeCallback = [](void* data, int32_t len, void* user) {
                if (!data || len <= 0)
                    return;
                auto* st = static_cast<ProgressState*>(user);
                std::string perc(static_cast<char*>(data), static_cast<size_t>(len));
                try {
                    float value = std::stof(perc);
                    (*(st->cb))(value);
                }
                catch (...) {
                    st->logger->Log(LogLevel::Warning, "Failed to parse download progress: " + perc);
                }
            };

            auto response = CallWithJsonAndCallback(core_, "download_model", MakeModelParams(info_.name), *logger_,
                                                    +nativeCallback, &state);
            if (response.HasError()) {
                throw Exception("Error downloading model [" + info_.name + "]: " + response.error, *logger_);
            }
        }
        else {
            auto response = CallWithJson(core_, "download_model", MakeModelParams(info_.name), *logger_);
            if (response.HasError()) {
                throw Exception("Error downloading model [" + info_.name + "]: " + response.error, *logger_);
            }
        }
    }

    void ModelVariant::Load() {
        auto response = CallWithJson(core_, "load_model", MakeModelParams(info_.name), *logger_);
        if (response.HasError()) {
            throw Exception("Error loading model [" + info_.name + "]: " + response.error, *logger_);
        }
    }

    const std::filesystem::path& ModelVariant::GetPath() const {
        if (cachedPath_.empty()) {
            auto response = CallWithJson(core_, "get_model_path", MakeModelParams(info_.name), *logger_);
            if (response.HasError()) {
                throw Exception("Error getting model path [" + info_.name + "]: " + response.error, *logger_);
            }
            cachedPath_ = std::filesystem::path(response.data);
        }
        return cachedPath_;
    }

    const std::string& ModelVariant::GetId() const noexcept {
        return info_.id;
    }

    const std::string& ModelVariant::GetAlias() const noexcept {
        return info_.alias;
    }

    uint32_t ModelVariant::GetVersion() const noexcept {
        return info_.version;
    }

    IModel::CoreAccess ModelVariant::GetCoreAccess() const {
        return {core_, info_.name, logger_};
    }

    /// Model

    Model::Model(gsl::not_null<Internal::IFoundryLocalCore*> core, gsl::not_null<ILogger*> logger)
        : core_(core), logger_(logger) {}

    ModelVariant& Model::SelectedVariant() {
        if (!selectedVariant_) {
            throw Exception("Model has no selected variant", *logger_);
        }
        return *const_cast<ModelVariant*>(selectedVariant_);
    }

    const ModelVariant& Model::SelectedVariant() const {
        if (!selectedVariant_) {
            throw Exception("Model has no selected variant", *logger_);
        }
        return *selectedVariant_;
    }

    gsl::span<const ModelVariant> Model::GetAllModelVariants() const {
        return variants_;
    }

    const ModelVariant& Model::GetLatestVersion(const ModelVariant& variant) const {
        const auto& targetName = variant.GetInfo().name;

        for (const auto& v : variants_) {
            // The variants returned by the catalog are sorted by version, so the first match should always be the
            // latest version.
            if (v.GetInfo().name == targetName) {
                return v;
            }
        }

        throw Exception("Model " + GetAlias() + " does not have a " + variant.GetId() + " variant.", *logger_);
    }

    const std::string& Model::GetId() const {
        return SelectedVariant().GetId();
    }

    const std::string& Model::GetAlias() const {
        return SelectedVariant().GetAlias();
    }

    void Model::SelectVariant(const ModelVariant& variant) const {
        const auto& targetId = variant.GetId();
        auto it = std::find_if(variants_.begin(), variants_.end(),
                               [&](const ModelVariant& v) { return v.GetId() == targetId; });

        if (it == variants_.end()) {
            throw Exception("Model " + GetAlias() + " does not have a " + variant.GetId() + " variant.", *logger_);
        }

        selectedVariant_ = &(*it);
    }

    IModel::CoreAccess Model::GetCoreAccess() const {
        return SelectedVariant().GetCoreAccess();
    }

} // namespace foundry_local
