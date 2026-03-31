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

namespace FoundryLocal::Internal {
    struct IFoundryLocalCore;
}

namespace FoundryLocal {
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

        static std::unique_ptr<Catalog> Create(gsl::not_null<FoundryLocal::Internal::IFoundryLocalCore*> core,
                                               gsl::not_null<ILogger*> logger) {
            return std::unique_ptr<Catalog>(new Catalog(core, logger));
        }

        const std::string& GetName() const { return name_; }
        std::vector<const Model*> ListModels() const;
        std::vector<const ModelVariant*> GetLoadedModels() const;
        std::vector<const ModelVariant*> GetCachedModels() const;

        const Model* GetModel(std::string_view modelId) const;
        const ModelVariant* GetModelVariant(std::string_view modelVariantId) const;

    private:
        void UpdateModels() const;

        mutable std::chrono::steady_clock::time_point lastFetch_{};

        mutable std::unordered_map<std::string, Model> byAlias_;
        mutable std::unordered_map<std::string, ModelVariant> modelIdToModelVariant_;

        explicit Catalog(gsl::not_null<FoundryLocal::Internal::IFoundryLocalCore*> injected,
                         gsl::not_null<ILogger*> logger);

        gsl::not_null<FoundryLocal::Internal::IFoundryLocalCore*> core_;
        std::string name_;
        gsl::not_null<ILogger*> logger_;

        friend class FoundryLocalManager;
#ifdef FL_TESTS
        friend struct Testing::MockObjectFactory;
#endif
    };

} // namespace FoundryLocal
