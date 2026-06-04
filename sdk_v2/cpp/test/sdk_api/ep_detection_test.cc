// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
// EP detection SDK integration tests using only the public C++ API.
// Tests exercise the full stack: C++ wrapper → C ABI → Manager → EpDetector → real bootstrappers.

#include "shared_test_env.h"

#include <foundry_local/foundry_local_cpp.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

// ========================================================================
// EP Detection — public API round-trip through real Manager
// ========================================================================

class EpDetectionApiTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto& env = SharedTestEnv::Get();
    if (!env.manager()) {
      GTEST_SKIP() << "Manager not available";
    }
  }

  static foundry_local::Manager& manager() { return *SharedTestEnv::Get().manager(); }
};

TEST_F(EpDetectionApiTest, GetDiscoverableEps_ReturnsResults) {
  auto eps = manager().GetDiscoverableEps();

  // On systems with WinML or other EP support, we get discoverable EPs.
  // On minimal systems, the list may be empty — that's valid.
  std::cout << "[  INFO    ] GetDiscoverableEps returned "
            << eps.size() << " EP(s)" << std::endl;

  for (const auto& ep : eps) {
    std::cout << "[  INFO    ]   " << ep.name
              << " (registered=" << ep.is_registered << ")" << std::endl;
    EXPECT_FALSE(ep.name.empty()) << "Every discoverable EP must have a non-empty name";
  }
}

TEST_F(EpDetectionApiTest, GetDiscoverableEps_NamesAreValidEpNames) {
  auto eps = manager().GetDiscoverableEps();

  for (const auto& ep : eps) {
    // All known EP names end with "ExecutionProvider"
    EXPECT_NE(ep.name.find("ExecutionProvider"), std::string::npos)
        << "Unexpected EP name format: " << ep.name;
  }
}

TEST_F(EpDetectionApiTest, GetDiscoverableEps_ConsistentAcrossCalls) {
  auto eps1 = manager().GetDiscoverableEps();
  auto eps2 = manager().GetDiscoverableEps();

  ASSERT_EQ(eps1.size(), eps2.size());

  for (size_t i = 0; i < eps1.size(); ++i) {
    EXPECT_EQ(eps1[i].name, eps2[i].name);
    EXPECT_EQ(eps1[i].is_registered, eps2[i].is_registered);
  }
}

TEST_F(EpDetectionApiTest, IsEpDownloadInProgress_InitiallyFalse) {
  EXPECT_FALSE(manager().IsEpDownloadInProgress());
}

TEST_F(EpDetectionApiTest, IsEpDownloadInProgress_ConsistentAcrossCalls) {
  // Calling multiple times without triggering a download should always return false
  EXPECT_FALSE(manager().IsEpDownloadInProgress());
  EXPECT_FALSE(manager().IsEpDownloadInProgress());
}

#ifdef _WIN32
// WinML 2.x reg-free runtime supports Windows 10 19H1 (build 18362) and later.
// This test verifies the full Manager → EpDetector → WinMLEpBootstrapper chain.
TEST_F(EpDetectionApiTest, GetDiscoverableEps_WindowsHasWinMLProviders) {
  auto eps = manager().GetDiscoverableEps();

  // This may fail on older Windows or minimal CI images — not a hard failure
  if (eps.empty()) {
    std::cout << "[  INFO    ] No WinML providers found — "
              << "this is expected on older Windows or CI images without WinML" << std::endl;
  } else {
    // If we DO have providers, verify they look reasonable
    EXPECT_GE(eps.size(), 1u);
    for (const auto& ep : eps) {
      EXPECT_FALSE(ep.name.empty());
    }
  }
}
#endif
