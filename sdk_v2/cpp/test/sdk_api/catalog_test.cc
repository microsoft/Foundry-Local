// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
// Catalog-focused SDK integration tests using only the public C++ API.

#include "model_fixture.h"

#include <gsl/span>
#include <memory>

namespace {

const foundry_local::IModel* FindModelByAlias(
    gsl::span<const std::unique_ptr<foundry_local::IModel>> models,
    std::string_view alias) {
  for (const auto& model : models) {
    if (model->GetInfo().Alias() == alias) {
      return model.get();
    }
  }

  return nullptr;
}

}  // namespace

TEST_F(ModelFixture, CatalogGetNameReturnsNonEmptyString) {
  auto name = catalog().GetName();
  EXPECT_FALSE(name.empty()) << "Catalog name should not be empty";
}

TEST_F(ModelFixture, CatalogGetModelByAliasReturnsExpectedModelAndVariants) {
  auto model = catalog().GetModel(model_alias());
  ASSERT_NE(model, nullptr) << "Expected catalog lookup by alias to succeed for '" << model_alias() << "'";

  auto info = model->GetInfo();
  EXPECT_EQ(info.Alias(), model_alias());
  EXPECT_EQ(info.Name(), chat_model().GetInfo().Name());
  EXPECT_EQ(info.Id(), model_id());
  EXPECT_FALSE(info.Uri().empty());

  foundry_local::ModelList variants = model->GetVariants();
  ASSERT_GE(variants.Models().size(), 1u);

  bool found_expected_variant = false;
  for (const auto& variant : variants.Models()) {
    if (variant->GetInfo().Id() == model_id()) {
      found_expected_variant = true;
      break;
    }
  }

  EXPECT_TRUE(found_expected_variant)
      << "Expected alias lookup to expose variant '" << model_id() << "' in GetVariants()";
}

TEST_F(ModelFixture, CatalogGetModelReturnsNulloptForUnknownAlias) {
  auto model = catalog().GetModel("nonexistent_model_xyz");

  EXPECT_EQ(model, nullptr);
}

TEST_F(ModelFixture, CatalogGetModelVariantByIdReturnsExpectedVariant) {
  auto variant = catalog().GetModelVariant(model_id());
  ASSERT_NE(variant, nullptr) << "Expected catalog lookup by id to succeed for '" << model_id() << "'";

  auto info = variant->GetInfo();
  EXPECT_EQ(info.Id(), model_id());
  EXPECT_EQ(info.Alias(), model_alias());
  EXPECT_EQ(info.Name(), chat_model().GetInfo().Name());
  EXPECT_TRUE(variant->IsCached());

  std::string local_path(variant->GetPath());
  EXPECT_FALSE(local_path.empty());
  EXPECT_TRUE(fs::exists(local_path)) << "Expected cached variant path to exist: " << local_path;

  foundry_local::ModelList variants = variant->GetVariants();
  ASSERT_EQ(variants.Models().size(), 1u)
      << "A model returned by GetModelVariant(id) should only expose that single variant";
  EXPECT_EQ(variants.Models().front()->GetInfo().Id(), model_id());
}

TEST_F(ModelFixture, CatalogGetModelVariantReturnsNulloptForUnknownId) {
  auto variant = catalog().GetModelVariant("nonexistent_model_xyz:999");

  EXPECT_EQ(variant, nullptr);
}

TEST_F(ModelFixture, CatalogGetCachedModelsIncludesDownloadedModel) {
  foundry_local::ModelList cached_models = catalog().GetCachedModels();
  const auto& cached = cached_models.Models();

  ASSERT_GE(cached.size(), 1u) << "Expected at least one cached model after fixture download";

  const foundry_local::IModel* cached_model = FindModelByAlias(cached, model_alias());
  ASSERT_NE(cached_model, nullptr)
      << "Expected cached models list to contain the downloaded alias '" << model_alias() << "'";

  EXPECT_TRUE(cached_model->IsCached());
  EXPECT_EQ(cached_model->GetInfo().Id(), model_id());

  std::string local_path(cached_model->GetPath());
  EXPECT_FALSE(local_path.empty());
  EXPECT_TRUE(fs::exists(local_path)) << "Expected cached model path to exist: " << local_path;
}
