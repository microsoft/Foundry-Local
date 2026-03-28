#include <gtest/gtest.h>

#include "mock_core.h"
#include "mock_object_factory.h"
#include "parser.h"
#include "foundry_local_exception.h"

#include <nlohmann/json.hpp>

using namespace FoundryLocal;
using namespace FoundryLocal::Testing;

using Factory = MockObjectFactory;

class ModelVariantTest : public ::testing::Test {
protected:
    MockCore core_;
    NullLogger logger_;

    ModelVariant MakeVariant(std::string name = "test-model", std::string alias = "test-alias", uint32_t version = 1) {
        return Factory::CreateModelVariant(&core_, Factory::MakeModelInfo(name, alias, version), &logger_);
    }
};

TEST_F(ModelVariantTest, GetInfo) {
    auto variant = MakeVariant("my-model", "my-alias", 3);
    const auto& info = variant.GetInfo();
    EXPECT_EQ("my-model", info.name);
    EXPECT_EQ("my-alias", info.alias);
    EXPECT_EQ(3u, info.version);
}

TEST_F(ModelVariantTest, GetId) {
    auto variant = MakeVariant("my-model");
    EXPECT_EQ("my-model", variant.GetId());
}

TEST_F(ModelVariantTest, GetAlias) {
    auto variant = MakeVariant("name", "alias");
    EXPECT_EQ("alias", variant.GetAlias());
}

TEST_F(ModelVariantTest, GetVersion) {
    auto variant = MakeVariant("name", "alias", 5);
    EXPECT_EQ(5u, variant.GetVersion());
}

TEST_F(ModelVariantTest, IsLoaded_True) {
    core_.OnCall("list_loaded_models", R"(["test-model:v1"])");
    auto variant = MakeVariant("test-model");
    EXPECT_TRUE(variant.IsLoaded());
}

TEST_F(ModelVariantTest, IsLoaded_False) {
    core_.OnCall("list_loaded_models", R"(["other-model:v1"])");
    auto variant = MakeVariant("test-model");
    EXPECT_FALSE(variant.IsLoaded());
}

TEST_F(ModelVariantTest, IsLoaded_EmptyList) {
    core_.OnCall("list_loaded_models", R"([])");
    auto variant = MakeVariant("test-model");
    EXPECT_FALSE(variant.IsLoaded());
}

TEST_F(ModelVariantTest, IsCached_True) {
    core_.OnCall("get_cached_models", R"(["test-model:1"])");
    auto variant = MakeVariant("test-model");
    EXPECT_TRUE(variant.IsCached());
}

TEST_F(ModelVariantTest, IsCached_False) {
    core_.OnCall("get_cached_models", R"(["other-model:1"])");
    auto variant = MakeVariant("test-model");
    EXPECT_FALSE(variant.IsCached());
}

TEST_F(ModelVariantTest, Load_CallsCore) {
    core_.OnCall("load_model", "");
    auto variant = MakeVariant("test-model");
    variant.Load();
    EXPECT_EQ(1, core_.GetCallCount("load_model"));

    // Verify the data argument contains the model name
    auto parsed = nlohmann::json::parse(core_.GetLastDataArg("load_model"));
    EXPECT_EQ("test-model", parsed["Params"]["Model"].get<std::string>());
}

TEST_F(ModelVariantTest, Unload_CallsCore) {
    core_.OnCall("unload_model", "");
    auto variant = MakeVariant("test-model");
    variant.Unload();
    EXPECT_EQ(1, core_.GetCallCount("unload_model"));
}

TEST_F(ModelVariantTest, Unload_ThrowsOnError) {
    core_.OnCallThrow("unload_model", "unload failed");
    auto variant = MakeVariant("test-model");
    EXPECT_THROW(variant.Unload(), FoundryLocalException);
}

TEST_F(ModelVariantTest, Download_NoCallback) {
    core_.OnCall("get_cached_models", R"([])");
    core_.OnCall("download_model", "");
    auto variant = MakeVariant("test-model");
    variant.Download();
    EXPECT_EQ(1, core_.GetCallCount("download_model"));
}

TEST_F(ModelVariantTest, Download_WithCallback) {
    core_.OnCall("get_cached_models", R"([])");
    core_.OnCall("download_model",
                 [](std::string_view, const std::string*, void* callback, void* userData) -> std::string {
                     // Simulate calling the progress callback
                     if (callback && userData) {
                         auto cb = reinterpret_cast<void (*)(void*, int32_t, void*)>(callback);
                         std::string progress = "50";
                         cb(progress.data(), static_cast<int32_t>(progress.size()), userData);
                     }
                     return "";
                 });

    auto variant = MakeVariant("test-model");
    float lastProgress = -1.0f;
    variant.Download([&](float pct) { lastProgress = pct; });
    EXPECT_NEAR(50.0f, lastProgress, 0.01f);
}

TEST_F(ModelVariantTest, RemoveFromCache_CallsCore) {
    core_.OnCall("remove_cached_model", "");
    auto variant = MakeVariant("test-model");
    variant.RemoveFromCache();
    EXPECT_EQ(1, core_.GetCallCount("remove_cached_model"));
}

TEST_F(ModelVariantTest, RemoveFromCache_ThrowsOnError) {
    core_.OnCallThrow("remove_cached_model", "remove failed");
    auto variant = MakeVariant("test-model");
    EXPECT_THROW(variant.RemoveFromCache(), FoundryLocalException);
}

TEST_F(ModelVariantTest, GetPath_CallsCore) {
    core_.OnCall("get_model_path", R"(C:\models\test)");
    auto variant = MakeVariant("test-model");
    const auto& path = variant.GetPath();
    EXPECT_EQ(std::filesystem::path(R"(C:\models\test)"), path);
}

TEST_F(ModelVariantTest, GetPath_CachesResult) {
    core_.OnCall("get_model_path", R"(C:\models\test)");
    auto variant = MakeVariant("test-model");
    variant.GetPath();
    variant.GetPath();
    // Should only call once due to caching
    EXPECT_EQ(1, core_.GetCallCount("get_model_path"));
}

class ModelTest : public ::testing::Test {
protected:
    MockCore core_;
    NullLogger logger_;

    Model MakeModel() { return Factory::CreateModel(&core_, &logger_); }

    ModelVariant MakeVariant(std::string name = "test-model", std::string alias = "test-alias", uint32_t version = 1) {
        return Factory::CreateModelVariant(&core_, Factory::MakeModelInfo(name, alias, version), &logger_);
    }

    /// Helper: create a Model with one variant and selectedVariantIndex_=0.
    Model MakeModelWithVariant(const std::string& name = "test-model", const std::string& alias = "test-alias") {
        auto model = MakeModel();
        Factory::AddVariantToModel(model, MakeVariant(name, alias, 1));
        Factory::SetSelectedVariantIndex(model, 0);
        return model;
    }
};

TEST_F(ModelTest, SelectedVariant_ThrowsWhenEmpty) {
    auto model = MakeModel();
    EXPECT_THROW(model.GetId(), FoundryLocalException);
}

TEST_F(ModelTest, AddVariant_AndSelect) {
    auto model = MakeModel();
    Factory::AddVariantToModel(model, MakeVariant("v1", "alias", 1));
    Factory::SetSelectedVariantIndex(model, 0);

    EXPECT_EQ("v1", model.GetId());
    EXPECT_EQ("alias", model.GetAlias());
}

TEST_F(ModelTest, GetAllModelVariants) {
    auto model = MakeModel();
    Factory::AddVariantToModel(model, MakeVariant("v1", "alias", 1));
    Factory::AddVariantToModel(model, MakeVariant("v2", "alias", 2));
    Factory::SetSelectedVariantIndex(model, 0);

    auto variants = model.GetAllModelVariants();
    EXPECT_EQ(2u, variants.size());
}

TEST_F(ModelTest, SelectVariant) {
    auto model = MakeModel();
    Factory::AddVariantToModel(model, MakeVariant("v1", "alias", 1));
    Factory::AddVariantToModel(model, MakeVariant("v2", "alias", 2));
    Factory::SetSelectedVariantIndex(model, 0);

    const auto* v2 = &model.GetAllModelVariants()[1];
    model.SelectVariant(v2);
    EXPECT_EQ("v2", model.GetId());
}

TEST_F(ModelTest, SelectVariant_NotFound_Throws) {
    auto model = MakeModel();
    Factory::AddVariantToModel(model, MakeVariant("v1", "alias", 1));
    Factory::SetSelectedVariantIndex(model, 0);

    auto external = MakeVariant("external", "alias", 1);
    EXPECT_THROW(model.SelectVariant(&external), FoundryLocalException);
}

TEST_F(ModelTest, GetLatestVariant) {
    auto model = MakeModel();
    Factory::AddVariantToModel(model, MakeVariant("target-model", "alias", 1));
    Factory::AddVariantToModel(model, MakeVariant("target-model", "alias", 2));
    Factory::SetSelectedVariantIndex(model, 0);

    const auto* first = &model.GetAllModelVariants()[0];
    const auto* latest = model.GetLatestVariant(first);
    // Should return the first one with matching name (which is variants_[0])
    EXPECT_EQ(first, latest);
}

TEST_F(ModelTest, DelegationMethods) {
    // Test that Model delegates to SelectedVariant
    core_.OnCall("list_loaded_models", R"(["test-model:v1"])");
    core_.OnCall("get_cached_models", R"(["test-model:1"])");
    core_.OnCall("load_model", "");
    core_.OnCall("unload_model", "");
    core_.OnCall("download_model", "");
    core_.OnCall("get_model_path", R"(C:\test)");

    auto model = MakeModelWithVariant("test-model", "alias");

    EXPECT_TRUE(model.IsLoaded());
    EXPECT_TRUE(model.IsCached());
    model.Load();
    model.Unload();
    model.Download();
    EXPECT_EQ(std::filesystem::path(R"(C:\test)"), model.GetPath());
}
