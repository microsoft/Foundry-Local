---
description: "Use when adding a new test fixture, modality, or model dependency to the C++ integration test suite, or when debugging unexpected GTEST_SKIP messages in CI."
applyTo: "sdk_v2/cpp/test/**"
---
# C++ Test CI Policy

## No model downloads in CI

The integration test suite must never pull a multi-GB model over the network during a CI run. The gate is centralized in `SharedTestEnv::AcquireModels()` (`sdk_v2/cpp/test/sdk_api/shared_test_env.h`):

- `fl::test::IsRunningInCI()` (defined in `test/internal_api/test_model_cache.h`) returns true when `TF_BUILD=true` (Azure DevOps) or `GITHUB_ACTIONS=true` (GitHub Actions), case-insensitive — mirrors the C# `IsRunningInCI()` helper.
- When in CI **and** `model.IsCached()` is false, the model is left out of `acquired_`. Per-test `SetUp()` then sees the modality accessor return `nullptr` and calls `GTEST_SKIP()`.
- `FOUNDRY_TEST_DATA_DIR` populates the cache. The path is passed to `Configuration::SetModelCacheDir()`, and `LocalModelScanner` finds models by `genai_config.json` + `inference_model.json` regardless of the `{publisher}/` subdirectory layout.

## Authoring rules

- **New modalities** must declare their need via `SharedTestEnv::AcquireModels({Modality::X})` in `SetUpTestSuite()` and check the accessor in `SetUp()` with `GTEST_SKIP()` on null. Do not call `model.Download()` or `model.Load()` directly from a test fixture.
- **Models that don't fit on CI agents** (vision, large embeddings, GPU-only variants) are expected to skip in CI. That is the design, not a bug — fixtures are already structured to handle it cleanly.
- **Do not add a `FOUNDRY_LOCAL_TEST_ALLOW_DOWNLOAD` escape hatch** without an architectural review. Tests that genuinely exercise download behavior (`DISABLED_DownloadFixture`) are gated by GTest's `DISABLED_` prefix and must be opted in explicitly.
- **Do not bleed CI policy into production SDK code.** No `Configuration::SetReadOnlyCache()` or `DownloadManager` mode flags — this is a test-policy decision.

## Debugging skips in CI

If model-using tests skip unexpectedly in CI, check the `SharedTestEnv: CI detected` banner in stdout — it reports the value of `FOUNDRY_TEST_DATA_DIR`. `(unset; all model-using tests will skip)` means the CI agent didn't mount the shared model cache. A specific `SharedTestEnv: skipping <model> in CI` line means the cache is mounted but that particular model isn't pre-staged.
