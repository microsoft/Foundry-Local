// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <memory>
#include <mutex>

#include <gsl/pointers>
#include <gsl/span>

#include "model.h"

namespace foundry_local::Internal {
    struct IFoundryLocalCore;
}

namespace foundry_local {

class Catalog final {
public:
        Catalog(const Catalog&) = delete;
        Catalog& operator=(const Catalog&) = delete;
        Catalog(Catalog&&) = delete;
        Catalog& operator=(Catalog&&) = delete;

        explicit Catalog(gsl::not_null<foundry_local::Internal::IFoundryLocalCore*> injected,
                         gsl::not_null<ILogger*> logger);

        static std::unique_ptr<Catalog> Create(gsl::not_null<foundry_local::Internal::IFoundryLocalCore*> core,
                                               gsl::not_null<ILogger*> logger) {
            return std::make_unique<Catalog>(core, logger);
        }

        const std::string& GetName() const { return name_; }
        std::vector<Model*> ListModels() const;
        std::vector<ModelVariant*> GetLoadedModels() const;
        std::vector<ModelVariant*> GetCachedModels() const;

        Model* GetModel(std::string_view modelId) const;
        ModelVariant* GetModelVariant(std::string_view modelVariantId) const;
        IModel& GetLatestVersion(const IModel& modelOrModelVariant) const;

    private:
        struct CatalogState {
            std::unordered_map<std::string, Model> byAlias;
            std::unordered_map<std::string, ModelVariant*> modelIdToModelVariant;
            std::chrono::steady_clock::time_point lastFetch{};
        };

        void UpdateModels() const;
        std::shared_ptr<const CatalogState> GetState() const;

        mutable std::mutex mutex_;
        mutable std::shared_ptr<const CatalogState> state_;

        gsl::not_null<foundry_local::Internal::IFoundryLocalCore*> core_;
        std::string name_;
        gsl::not_null<ILogger*> logger_;
    };

} // namespace foundry_local
