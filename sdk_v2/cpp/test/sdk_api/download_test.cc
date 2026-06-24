// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
// Download integration tests — exercises RemoveFromCache + Download code paths.
// Disabled by default because they modify the model cache and require network
// access.  Run with --gtest_also_run_disabled_tests to include them.

#include "model_fixture.h"

#include <iostream>
#include <vector>

// ========================================================================
// DISABLED_DownloadFixture — exercises RemoveFromCache + Download.
// Disabled by default because it modifies the model cache and requires
// network access.  Run with --gtest_also_run_disabled_tests to include
// (e.g. during code-coverage runs).
// ========================================================================

class DISABLED_DownloadFixture : public ::testing::Test {
 protected:
  void SetUp() override {
    auto& env = SharedTestEnv::Get();
    if (!env.model_list()) {
      GTEST_SKIP() << "No catalog available";
    }
  }

  static foundry_local::ModelList& model_list() {
    return *SharedTestEnv::Get().model_list();
  }
};

TEST_F(DISABLED_DownloadFixture, RemoveAndRedownloadSmallestModel) {
  // Find the smallest CPU model that is NOT already loaded by SharedTestEnv.
  // Iterate all variants of each alias group — GetModels() only returns the
  // selected variant, which may hide smaller CPU variants behind a GPU pick.
  foundry_local::IModel* target = nullptr;
  int64_t target_size = std::numeric_limits<int64_t>::max();

  for (const auto& m : model_list()) {
    if (m->IsLoaded()) {
      continue;
    }

    auto variants = m->GetVariants();
    for (const auto& v : variants) {
      auto vi = v->GetInfo();

      if (vi.DeviceType() != FOUNDRY_LOCAL_DEVICE_CPU) {
        continue;
      }

      int64_t size = vi.FilesizeMb().value_or(0);
      if (size > 0 && size < target_size) {
        target_size = size;
        m->SelectVariant(*v);
        target = m.get();
      }
    }
  }

  ASSERT_NE(target, nullptr) << "No unloaded CPU model found in catalog";

  auto info = target->GetInfo();
  std::cout << "Download test model: " << info.Name()
            << " (" << info.FilesizeMb().value_or(0) << " MB)\n";

  // Remove from cache to force a real download.
  if (target->IsCached()) {
    target->RemoveFromCache();
  }

  ASSERT_FALSE(target->IsCached())
      << "Model should not be cached after RemoveFromCache";

  // Download with progress tracking.
  std::vector<float> progress_values;
  target->Download([&progress_values](float pct) {
    progress_values.push_back(pct);
    return 0;  // 0 = continue (non-zero would cancel; see flProgressCallback contract).
  });

  EXPECT_TRUE(target->IsCached())
      << "Model should be cached after Download";

  // A real download should produce multiple progress callbacks — not just
  // a single 100% that a cache hit would produce.
  ASSERT_GT(progress_values.size(), 1u)
      << "Expected multiple progress callbacks from a fresh download";

  // Final progress should be 100%.
  EXPECT_FLOAT_EQ(progress_values.back(), 100.0f);

  // Verify progress values are monotonically non-decreasing.
  for (size_t i = 1; i < progress_values.size(); ++i) {
    EXPECT_GE(progress_values[i], progress_values[i - 1])
        << "Progress went backwards at index " << i;
  }

  std::cout << "Download completed with " << progress_values.size()
            << " progress callbacks\n";

  // Load and verify the downloaded model works.
  EXPECT_NO_THROW(target->Load());
  EXPECT_TRUE(target->IsLoaded());

  // Unload — this model isn't used by other fixtures.
  target->Unload();
  EXPECT_FALSE(target->IsLoaded());
}

TEST_F(DISABLED_DownloadFixture, DownloadAlreadyCachedModelIsNoOp) {
  // Grab the chat model that SharedTestEnv already downloaded.
  auto* model = SharedTestEnv::Get().chat_model();
  if (!model) {
    GTEST_SKIP() << "No chat model available";
  }

  ASSERT_TRUE(model->IsCached()) << "Chat model should already be cached";

  // Download again — should be a no-op but still call progress with 100%.
  std::vector<float> progress_values;
  model->Download([&progress_values](float pct) {
    progress_values.push_back(pct);
    return 0;  // 0 = continue (non-zero would cancel; see flProgressCallback contract).
  });

  EXPECT_TRUE(model->IsCached());

  // For an already-cached model the callback should still fire with 100%.
  ASSERT_FALSE(progress_values.empty());
  EXPECT_FLOAT_EQ(progress_values.back(), 100.0f);
}
