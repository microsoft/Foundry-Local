// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#ifndef FL_TESTS
#define FL_TESTS
#endif

#include "foundry_local.h"
#include "foundry_local_internal_core.h"
#include "logger.h"

namespace foundry_local::Testing {

    /// Factory to construct private-constructor types for testing.
    /// Declared as a friend (Testing::MockObjectFactory) in ModelVariant, Model, and Catalog when FL_TESTS is defined.
    struct MockObjectFactory {
        static ModelVariant CreateModelVariant(gsl::not_null<Internal::IFoundryLocalCore*> core, ModelInfo info,
                                               gsl::not_null<ILogger*> logger) {
            return ModelVariant(core, std::move(info), logger);
        }

        static std::unique_ptr<Catalog> CreateCatalog(gsl::not_null<Internal::IFoundryLocalCore*> core,
                                                      gsl::not_null<ILogger*> logger) {
            return std::unique_ptr<Catalog>(new Catalog(core, logger));
        }

        static Model CreateModel(gsl::not_null<Internal::IFoundryLocalCore*> core, gsl::not_null<ILogger*> logger) {
            return Model(core, logger);
        }

        /// Push a variant into a Model's internal variant list.
        static void AddVariantToModel(Model& model, ModelVariant variant) {
            model.variants_.push_back(std::move(variant));
        }

        /// Set the selected variant on a Model.
        static void SelectFirstVariant(Model& model) { model.selectedVariant_ = &model.variants_.front(); }

        /// Helper to build a minimal ModelInfo with defaults.
        static ModelInfo MakeModelInfo(std::string name, std::string alias = "", uint32_t version = 1) {
            ModelInfo info;
            info.id = name + ":" + std::to_string(version);
            info.name = std::move(name);
            info.alias = alias.empty() ? info.name : std::move(alias);
            info.version = version;
            info.provider_type = "test";
            info.uri = "test://uri";
            info.model_type = "text";
            return info;
        }

        /// Helper to build a JSON string representing a model list entry.
        static std::string MakeModelInfoJson(const std::string& name, const std::string& alias = "",
                                             uint32_t version = 1, bool cached = false) {
            std::string a = alias.empty() ? name : alias;
            std::string id = name + ":" + std::to_string(version);
            return R"({"id":")" + id + R"(","name":")" + name + R"(","version":)" + std::to_string(version) +
                   R"(,"alias":")" + a + R"(","providerType":"test","uri":"test://uri","modelType":"text","cached":)" +
                   (cached ? "true" : "false") + R"(,"createdAt":0})";
        }
    };

} // namespace foundry_local::Testing
