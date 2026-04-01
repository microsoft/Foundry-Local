// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <string>
#include <string_view>
#include <vector>
#include <chrono>

#include <gsl/span>
#include <nlohmann/json.hpp>

#include "foundry_local.h"
#include "foundry_local_internal_core.h"
#include "foundry_local_exception.h"
#include "core_helpers.h"
#include "parser.h"
#include "logger.h"

namespace foundry_local {

using namespace detail;

Catalog::Catalog(gsl::not_null<Internal::IFoundryLocalCore*> injected, gsl::not_null<ILogger*> logger)
    : core_(injected), logger_(logger) {
    auto response = core_->call("get_catalog_name", *logger_, /*dataArgument*/ nullptr);
    if (response.HasError()) {
        throw Exception(std::string("Error getting catalog name: ") + response.error, *logger_);
    }
    name_ = std::move(response.data);
}

std::vector<ModelVariant*> Catalog::GetLoadedModels() const {
    UpdateModels();
    return CollectVariantsByIds(modelIdToModelVariant_, GetLoadedModelsInternal(core_, *logger_));
}

std::vector<ModelVariant*> Catalog::GetCachedModels() const {
    UpdateModels();
    return CollectVariantsByIds(modelIdToModelVariant_, GetCachedModelsInternal(core_, *logger_));
}

Model* Catalog::GetModel(std::string_view modelId) const {
    UpdateModels();
    auto it = byAlias_.find(std::string(modelId));
    if (it != byAlias_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<Model*> Catalog::ListModels() const {
    UpdateModels();

    std::vector<Model*> out;
    out.reserve(byAlias_.size());
    for (auto& kv : byAlias_)
        out.emplace_back(&kv.second);

    return out;
}

void Catalog::UpdateModels() const {
    using clock = std::chrono::steady_clock;

    // TODO: make this configurable
    constexpr auto kRefreshInterval = std::chrono::hours(6);

    const auto now = clock::now();
    if (lastFetch_.time_since_epoch() != clock::duration::zero() && (now - lastFetch_) < kRefreshInterval) {
        return;
    }

    const auto response = core_->call("get_model_list", *logger_);
    if (response.HasError()) {
        throw Exception(std::string("Error getting model list: ") + response.error, *logger_);
    }
    const auto arr = nlohmann::json::parse(response.data);

    byAlias_.clear();
    modelIdToModelVariant_.clear();

    for (const auto& j : arr) {
        const std::string alias = j.at("alias").get<std::string>();

        auto it = byAlias_.find(alias);
        if (it == byAlias_.end()) {
            Model m(core_, logger_);
            it = byAlias_.emplace(alias, std::move(m)).first;
        }

        ModelInfo modelVariantInfo;
        from_json(j, modelVariantInfo);
        std::string variantId = modelVariantInfo.id;
        ModelVariant modelVariant(core_, modelVariantInfo, logger_);
        modelIdToModelVariant_.emplace(variantId, modelVariant);

        it->second.variants_.emplace_back(std::move(modelVariant));
    }

    // Auto-select the first variant for each model.
    for (auto& [alias, model] : byAlias_) {
        if (!model.variants_.empty()) {
            model.selectedVariant_ = &model.variants_.front();
        }
    }

    lastFetch_ = now;
}

ModelVariant* Catalog::GetModelVariant(std::string_view id) const {
    UpdateModels();
    auto it = modelIdToModelVariant_.find(std::string(id));
    if (it != modelIdToModelVariant_.end()) {
        return &it->second;
    }
    return nullptr;
}

} // namespace foundry_local
