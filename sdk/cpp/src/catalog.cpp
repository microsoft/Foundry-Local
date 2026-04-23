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
        : state_(std::make_shared<const CatalogState>()), core_(injected), logger_(logger) {
        auto response = core_->call("get_catalog_name", *logger_, /*dataArgument*/ nullptr);
        if (response.HasError()) {
            throw Exception(std::string("Error getting catalog name: ") + response.error, *logger_);
        }
        name_ = std::move(response.data);
    }

    std::shared_ptr<const Catalog::CatalogState> Catalog::GetState() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_;
    }

    std::vector<IModel*> Catalog::GetLoadedModels() const {
        UpdateModels();
        auto state = GetState();
        return CollectVariantsByIds(state->modelIdToModelVariant, GetLoadedModelsInternal(core_, *logger_));
    }

    std::vector<IModel*> Catalog::GetCachedModels() const {
        UpdateModels();
        auto state = GetState();
        return CollectVariantsByIds(state->modelIdToModelVariant, GetCachedModelsInternal(core_, *logger_));
    }

    IModel* Catalog::GetModel(std::string_view modelId) const {
        UpdateModels();
        auto state = GetState();
        auto it = state->byAlias.find(std::string(modelId));
        if (it != state->byAlias.end()) {
            return const_cast<Model*>(&it->second);
        }
        return nullptr;
    }

    std::vector<IModel*> Catalog::ListModels() const {
        UpdateModels();
        auto state = GetState();

        std::vector<IModel*> out;
        out.reserve(state->byAlias.size());
        for (auto& kv : state->byAlias)
            out.emplace_back(const_cast<Model*>(&kv.second));

        return out;
    }

    void Catalog::UpdateModels() const {
        using clock = std::chrono::steady_clock;

        // TODO: make this configurable
        constexpr auto kRefreshInterval = std::chrono::hours(6);

        const auto now = clock::now();
        {
            auto current = GetState();
            if (current->lastFetch.time_since_epoch() != clock::duration::zero() &&
                (now - current->lastFetch) < kRefreshInterval) {
                return;
            }
        }

        // Fetch outside the lock so the core call doesn't block readers.
        const auto response = core_->call("get_model_list", *logger_);
        if (response.HasError()) {
            throw Exception(std::string("Error getting model list: ") + response.error, *logger_);
        }
        const auto arr = nlohmann::json::parse(response.data);

        // Build the new state locally no reader can see partial data.
        auto newState = std::make_shared<CatalogState>();

        for (const auto& j : arr) {
            const std::string alias = j.at("alias").get<std::string>();

            auto it = newState->byAlias.find(alias);
            if (it == newState->byAlias.end()) {
                Model m(core_, logger_);
                it = newState->byAlias.emplace(alias, std::move(m)).first;
            }

            ModelInfo modelVariantInfo;
            from_json(j, modelVariantInfo);
            ModelVariant modelVariant(core_, modelVariantInfo, logger_);
            it->second.variants_.emplace_back(std::move(modelVariant));
        }

        // Build the lookup map from pointers into the owning Model::variants_ vectors,
        // and auto-select the first variant for each model.
        for (auto& [alias, model] : newState->byAlias) {
            for (auto& variant : model.variants_) {
                newState->modelIdToModelVariant.emplace(variant.GetId(), &variant);
            }
            if (!model.variants_.empty()) {
                model.selectedVariant_ = &model.variants_.front();
            }
        }

        newState->lastFetch = now;

        // Atomic swap — readers that already hold the old shared_ptr keep it alive.
        {
            std::lock_guard<std::mutex> lock(mutex_);
            state_ = std::move(newState);
        }
    }

    IModel* Catalog::GetModelVariant(std::string_view id) const {
        UpdateModels();
        auto state = GetState();
        auto it = state->modelIdToModelVariant.find(std::string(id));
        if (it != state->modelIdToModelVariant.end()) {
            return it->second;
        }
        return nullptr;
    }

    IModel& Catalog::GetLatestVersion(const IModel& modelOrModelVariant) const {
        const auto& alias = modelOrModelVariant.GetAlias();
        auto* imodel = GetModel(alias);
        if (!imodel) {
            throw Exception("Model " + alias + " not found in catalog.", *logger_);
        }

        auto* model = dynamic_cast<Model*>(imodel);
        if (!model) {
            throw Exception("Model " + alias + " is not a Model instance.", *logger_);
        }

        // Resolve the variant name from the IModel's ID by looking it up in the catalog.
        const auto& id = modelOrModelVariant.GetId();
        auto state = GetState();
        auto it = state->modelIdToModelVariant.find(id);
        if (it == state->modelIdToModelVariant.end()) {
            throw Exception("Model " + alias + " does not have a " + id + " variant.", *logger_);
        }

        const auto& targetName = it->second->GetInfo().name;
        for (auto& v : model->GetAllModelVariants()) {
            // The variants returned by the catalog are sorted by version, so the first match should always be the
            // latest version.
            if (v.GetInfo().name == targetName) {
                return const_cast<ModelVariant&>(v);
            }
        }

        throw Exception("Model " + alias + " does not have a " + id + " variant.", *logger_);
    }

} // namespace foundry_local
