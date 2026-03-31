// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <memory>

#include <gsl/pointers>
#include <gsl/span>

#include "model.h"

namespace foundry_local::Internal {
    struct IFoundryLocalCore;
}

namespace foundry_local {
#ifdef FL_TESTS
    namespace Testing {
        struct MockObjectFactory;
    }
#endif

    class Catalog final {
    public:
        Catalog(const Catalog&) = delete;
        Catalog& operator=(const Catalog&) = delete;
        Catalog(Catalog&&) = delete;
        Catalog& operator=(Catalog&&) = delete;

        static std::unique_ptr<Catalog> Create(gsl::not_null<foundry_local::Internal::IFoundryLocalCore*> core,
                                               gsl::not_null<ILogger*> logger) {
            return std::unique_ptr<Catalog>(new Catalog(core, logger));
        }

        const std::string& GetName() const { return name_; }
        std::vector<Model*> ListModels() const;
        std::vector<ModelVariant*> GetLoadedModels() const;
        std::vector<ModelVariant*> GetCachedModels() const;

        Model* GetModel(std::string_view modelId) const;
        ModelVariant* GetModelVariant(std::string_view modelVariantId) const;

    private:
        void UpdateModels() const;

        mutable std::chrono::steady_clock::time_point lastFetch_{};

        mutable std::unordered_map<std::string, Model> byAlias_;
        mutable std::unordered_map<std::string, ModelVariant> modelIdToModelVariant_;

        explicit Catalog(gsl::not_null<foundry_local::Internal::IFoundryLocalCore*> injected,
                         gsl::not_null<ILogger*> logger);

        gsl::not_null<foundry_local::Internal::IFoundryLocalCore*> core_;
        std::string name_;
        gsl::not_null<ILogger*> logger_;

        friend class FoundryLocalManager;
#ifdef FL_TESTS
        friend struct Testing::MockObjectFactory;
#endif
    };

} // namespace foundry_local
