// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "mock_core.h"
#include "mock_object_factory.h"
#include "parser.h"
#include "foundry_local_exception.h"

#include <nlohmann/json.hpp>

using namespace foundry_local;
using namespace foundry_local::Testing;

using Factory = MockObjectFactory;

class CatalogTest : public ::testing::Test {
protected:
    MockCore core_;
    NullLogger logger_;

    std::string MakeModelListJson(const std::vector<std::pair<std::string, std::string>>& models) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& [name, alias] : models) {
            arr.push_back(nlohmann::json::parse(Factory::MakeModelInfoJson(name, alias)));
        }
        return arr.dump();
    }

    std::unique_ptr<Catalog> MakeCatalog() {
        core_.OnCall("get_catalog_name", "test-catalog");
        return Factory::CreateCatalog(&core_, &logger_);
    }
};

TEST_F(CatalogTest, GetName) {
    auto catalog = MakeCatalog();
    EXPECT_EQ("test-catalog", catalog->GetName());
}

TEST_F(CatalogTest, Create_ThrowsOnCoreError) {
    core_.OnCallThrow("get_catalog_name", "catalog error");
    EXPECT_THROW(MockObjectFactory::CreateCatalog(&core_, &logger_), Exception);
}

TEST_F(CatalogTest, ListModels_Empty) {
    core_.OnCall("get_model_list", "[]");
    auto catalog = MakeCatalog();
    auto models = catalog->ListModels();
    EXPECT_TRUE(models.empty());
}

TEST_F(CatalogTest, ListModels_SingleModel) {
    core_.OnCall("get_model_list", MakeModelListJson({{"model-1", "my-model"}}));
    auto catalog = MakeCatalog();
    auto models = catalog->ListModels();
    ASSERT_EQ(1u, models.size());
    EXPECT_EQ("my-model", models[0]->GetAlias());
}

TEST_F(CatalogTest, ListModels_MultipleVariantsSameAlias) {
    // Two variants of the same model (same alias, different names)
    nlohmann::json arr = nlohmann::json::array();
    arr.push_back(nlohmann::json::parse(Factory::MakeModelInfoJson("model-v1", "my-model", 1)));
    arr.push_back(nlohmann::json::parse(Factory::MakeModelInfoJson("model-v2", "my-model", 2)));
    core_.OnCall("get_model_list", arr.dump());

    auto catalog = MakeCatalog();
    auto models = catalog->ListModels();

    // Should be grouped into one Model
    ASSERT_EQ(1u, models.size());
    EXPECT_EQ(2u, models[0]->GetAllModelVariants().size());
}

TEST_F(CatalogTest, ListModels_DifferentAliases) {
    core_.OnCall("get_model_list", MakeModelListJson({{"model-a", "alias-a"}, {"model-b", "alias-b"}}));
    auto catalog = MakeCatalog();
    auto models = catalog->ListModels();
    EXPECT_EQ(2u, models.size());
}

TEST_F(CatalogTest, ListModels_IncludesOpenAIPrefix) {
    core_.OnCall("get_model_list", MakeModelListJson({{"model-a", "my-model"}, {"openai-model", "openai-stuff"}}));
    auto catalog = MakeCatalog();
    auto models = catalog->ListModels();
    ASSERT_EQ(2u, models.size());
}

TEST_F(CatalogTest, GetModel_Found) {
    core_.OnCall("get_model_list", MakeModelListJson({{"model-1", "my-model"}}));
    auto catalog = MakeCatalog();

    auto* model = catalog->GetModel("my-model");
    ASSERT_NE(nullptr, model);
    EXPECT_EQ("my-model", model->GetAlias());
}

TEST_F(CatalogTest, GetModel_NotFound) {
    core_.OnCall("get_model_list", MakeModelListJson({{"model-1", "my-model"}}));
    auto catalog = MakeCatalog();

    EXPECT_EQ(nullptr, catalog->GetModel("nonexistent"));
}

TEST_F(CatalogTest, GetModelVariant_Found) {
    core_.OnCall("get_model_list", MakeModelListJson({{"model-1", "my-model"}}));
    auto catalog = MakeCatalog();

    auto* variant = catalog->GetModelVariant("model-1:1");
    ASSERT_NE(nullptr, variant);
    EXPECT_EQ("model-1:1", variant->GetId());
}

TEST_F(CatalogTest, GetModelVariant_NotFound) {
    core_.OnCall("get_model_list", MakeModelListJson({{"model-1", "my-model"}}));
    auto catalog = MakeCatalog();

    EXPECT_EQ(nullptr, catalog->GetModelVariant("nonexistent:1"));
}

TEST_F(CatalogTest, GetLoadedModels) {
    core_.OnCall("get_model_list", MakeModelListJson({{"model-1", "alias-1"}, {"model-2", "alias-2"}}));
    core_.OnCall("list_loaded_models", R"(["model-1:1"])");

    auto catalog = MakeCatalog();

    auto loaded = catalog->GetLoadedModels();
    ASSERT_EQ(1u, loaded.size());
    EXPECT_EQ("model-1:1", loaded[0]->GetId());
}

TEST_F(CatalogTest, GetCachedModels) {
    core_.OnCall("get_model_list", MakeModelListJson({{"model-1", "alias-1"}, {"model-2", "alias-2"}}));
    core_.OnCall("get_cached_models", R"(["model-1:1", "model-2:1"])");

    auto catalog = MakeCatalog();

    auto cached = catalog->GetCachedModels();
    EXPECT_EQ(2u, cached.size());
}

TEST_F(CatalogTest, ListModels_CachesResults) {
    core_.OnCall("get_model_list", MakeModelListJson({{"model-1", "my-model"}}));
    auto catalog = MakeCatalog();

    catalog->ListModels();
    catalog->ListModels();

    // Should only call get_model_list once due to caching
    EXPECT_EQ(1, core_.GetCallCount("get_model_list"));
}

class FileBasedCatalogTest : public ::testing::Test {
protected:
    NullLogger logger_;

    static std::string TestDataPath(const std::string& filename) { return "testdata/" + filename; }
};

TEST_F(FileBasedCatalogTest, RealModelsList) {
    auto core = FileBackedCore::FromModelList(TestDataPath("real_models_list.json"));
    auto catalog = Factory::CreateCatalog(&core, &logger_);

    auto models = catalog->ListModels();
    ASSERT_EQ(2u, models.size());

    int phi_models = 0, mistral_models = 0;
    size_t phi_variants = 0, mistral_variants = 0;

    for (const auto* model : models) {
        if (model->GetAlias() == "phi-4") {
            phi_models++;
            phi_variants = model->GetAllModelVariants().size();
        }
        else if (model->GetAlias() == "mistral-7b-v0.2") {
            mistral_models++;
            mistral_variants = model->GetAllModelVariants().size();
        }
    }

    EXPECT_EQ(1, phi_models);
    EXPECT_EQ(1, mistral_models);
    EXPECT_EQ(2u, phi_variants);
    EXPECT_EQ(2u, mistral_variants);
}

TEST_F(FileBasedCatalogTest, RealModelsList_VariantDetails) {
    auto core = FileBackedCore::FromModelList(TestDataPath("real_models_list.json"));
    auto catalog = Factory::CreateCatalog(&core, &logger_);

    const auto* gpuVariant = catalog->GetModelVariant("Phi-4-generic-gpu:1");
    ASSERT_NE(nullptr, gpuVariant);

    const auto& info = gpuVariant->GetInfo();
    EXPECT_EQ("Phi-4-generic-gpu:1", info.id);
    EXPECT_EQ("Phi-4-generic-gpu", info.name);
    EXPECT_EQ("phi-4", info.alias);
    ASSERT_TRUE(info.display_name.has_value());
    EXPECT_EQ("Phi-4 (GPU)", *info.display_name);
    ASSERT_TRUE(info.publisher.has_value());
    EXPECT_EQ("Microsoft", *info.publisher);
    ASSERT_TRUE(info.license.has_value());
    EXPECT_EQ("MIT", *info.license);
    ASSERT_TRUE(info.runtime.has_value());
    EXPECT_EQ(DeviceType::GPU, info.runtime->device_type);
    EXPECT_EQ("DML", info.runtime->execution_provider);
    ASSERT_TRUE(info.file_size_mb.has_value());
    EXPECT_EQ(8192u, *info.file_size_mb);
    ASSERT_TRUE(info.supports_tool_calling.has_value());
    EXPECT_TRUE(*info.supports_tool_calling);
    ASSERT_TRUE(info.max_output_tokens.has_value());
    EXPECT_EQ(4096, *info.max_output_tokens);
    ASSERT_TRUE(info.prompt_template.has_value());
    EXPECT_EQ("<|system|>", info.prompt_template->system);
    EXPECT_EQ("<|user|>", info.prompt_template->user);
    EXPECT_EQ("<|assistant|>", info.prompt_template->assistant);
    EXPECT_EQ("<|prompt|>", info.prompt_template->prompt);
}

TEST_F(FileBasedCatalogTest, RealModelsList_CpuVariantDetails) {
    auto core = FileBackedCore::FromModelList(TestDataPath("real_models_list.json"));
    auto catalog = Factory::CreateCatalog(&core, &logger_);

    const auto* cpuVariant = catalog->GetModelVariant("Phi-4-generic-cpu:1");
    ASSERT_NE(nullptr, cpuVariant);

    const auto& info = cpuVariant->GetInfo();
    EXPECT_EQ("Phi-4-generic-cpu", info.name);
    ASSERT_TRUE(info.runtime.has_value());
    EXPECT_EQ(DeviceType::CPU, info.runtime->device_type);
    EXPECT_EQ("ORT", info.runtime->execution_provider);
    ASSERT_TRUE(info.file_size_mb.has_value());
    EXPECT_EQ(4096u, *info.file_size_mb);
    ASSERT_TRUE(info.supports_tool_calling.has_value());
    EXPECT_FALSE(*info.supports_tool_calling);
    EXPECT_FALSE(info.prompt_template.has_value());
}

TEST_F(FileBasedCatalogTest, EmptyModelsList) {
    auto core = FileBackedCore::FromModelList(TestDataPath("empty_models_list.json"));
    auto catalog = Factory::CreateCatalog(&core, &logger_);

    auto models = catalog->ListModels();
    EXPECT_TRUE(models.empty());
}

TEST_F(FileBasedCatalogTest, MalformedJson) {
    auto core = FileBackedCore::FromModelList(TestDataPath("malformed_models_list.json"));
    auto catalog = Factory::CreateCatalog(&core, &logger_);

    EXPECT_ANY_THROW(catalog->ListModels());
}

TEST_F(FileBasedCatalogTest, MissingNameField) {
    auto core = FileBackedCore::FromModelList(TestDataPath("missing_name_field_models_list.json"));
    auto catalog = Factory::CreateCatalog(&core, &logger_);

    try {
        catalog->ListModels();
        FAIL() << "Expected exception for missing 'name' field";
    }
    catch (const std::exception& e) {
        std::string msg = e.what();
        EXPECT_NE(std::string::npos, msg.find("name")) << "Actual: " << msg;
    }
}

TEST_F(FileBasedCatalogTest, CachedModels) {
    auto core =
        FileBackedCore::FromBoth(TestDataPath("real_models_list.json"), TestDataPath("valid_cached_models.json"));
    auto catalog = Factory::CreateCatalog(&core, &logger_);

    auto cached = catalog->GetCachedModels();
    ASSERT_EQ(2u, cached.size());

    std::vector<std::string> names;
    names.reserve(cached.size());
    for (const auto* mv : cached)
        names.push_back(mv->GetInfo().name);

    EXPECT_NE(std::find(names.begin(), names.end(), "Phi-4-generic-gpu"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "Phi-4-generic-cpu"), names.end());
}

TEST_F(FileBasedCatalogTest, CoreErrorOnModelList) {
    auto core = FileBackedCore::FromModelList("testdata/nonexistent_file.json");
    auto catalog = Factory::CreateCatalog(&core, &logger_);

    EXPECT_ANY_THROW(catalog->ListModels());
}

TEST_F(FileBasedCatalogTest, MixedOpenAIAndLocal_IncludesAll) {
    auto core = FileBackedCore::FromModelList(TestDataPath("mixed_openai_and_local.json"));
    auto catalog = Factory::CreateCatalog(&core, &logger_);

    auto models = catalog->ListModels();
    ASSERT_EQ(3u, models.size());
}

TEST_F(FileBasedCatalogTest, ThreeVariantsOneModel) {
    auto core = FileBackedCore::FromModelList(TestDataPath("three_variants_one_model.json"));
    auto catalog = Factory::CreateCatalog(&core, &logger_);

    auto models = catalog->ListModels();
    ASSERT_EQ(1u, models.size());
    EXPECT_EQ(3u, models[0]->GetAllModelVariants().size());
}

TEST_F(FileBasedCatalogTest, ThreeVariantsOneModel_CachedSubset) {
    auto core = FileBackedCore::FromBoth(TestDataPath("three_variants_one_model.json"),
                                         TestDataPath("single_cached_model.json"));
    auto catalog = Factory::CreateCatalog(&core, &logger_);

    auto cached = catalog->GetCachedModels();
    ASSERT_EQ(1u, cached.size());
    EXPECT_EQ("multi-v1-cpu", cached[0]->GetInfo().name);
}

TEST_F(FileBasedCatalogTest, GetModelByAlias) {
    auto core = FileBackedCore::FromModelList(TestDataPath("real_models_list.json"));
    auto catalog = Factory::CreateCatalog(&core, &logger_);

    const auto* model = catalog->GetModel("phi-4");
    ASSERT_NE(nullptr, model);
    EXPECT_EQ("phi-4", model->GetAlias());
    EXPECT_EQ(2u, model->GetAllModelVariants().size());

    const auto* missing = catalog->GetModel("nonexistent-alias");
    EXPECT_EQ(nullptr, missing);
}

TEST_F(FileBasedCatalogTest, GetModelVariant_NotInCatalog) {
    auto core = FileBackedCore::FromModelList(TestDataPath("real_models_list.json"));
    auto catalog = Factory::CreateCatalog(&core, &logger_);

    EXPECT_EQ(nullptr, catalog->GetModelVariant("nonexistent-variant-id"));
}

TEST_F(FileBasedCatalogTest, LoadedModels) {
    auto core = FileBackedCore::FromAll(TestDataPath("real_models_list.json"), TestDataPath("valid_cached_models.json"),
                                        TestDataPath("valid_loaded_models.json"));
    auto catalog = Factory::CreateCatalog(&core, &logger_);

    auto loaded = catalog->GetLoadedModels();
    ASSERT_EQ(1u, loaded.size());
    EXPECT_EQ("Phi-4-generic-gpu", loaded[0]->GetInfo().name);
}
