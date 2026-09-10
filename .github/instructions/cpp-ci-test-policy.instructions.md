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

## Dynamic Engine lane

Engine-capable builds compile `DynamicEngineChatTest`. Set `FOUNDRY_LOCAL_DYNAMIC_ENGINE_TEST_MODEL_PATH` to a
pre-staged model directory whose `genai_config.json` defines `engine.dynamic_batching` to run generation,
continuation, concurrency, capacity, cancellation-recovery, and unload coverage. The fixture stages writable metadata
without modifying the shared model and sets `max_batch_size` to two. Required lanes must also set
`FOUNDRY_LOCAL_DYNAMIC_ENGINE_TEST_REQUIRED=1` and configure with
`FOUNDRY_LOCAL_REQUIRE_DYNAMIC_ENGINE_TESTS=ON`; the former converts a missing model fixture into a test failure, and
the latter rejects a GenAI package that cannot compile the suite. Current GenAI 0.15.2 builds exclude these tests, so
an Engine lane must use a newer package and a real pre-staged paged-KV model rather than relying on skips.

The packaging pipeline includes the unconditional `cpp_test_engine` stage from
`.pipelines/v2/templates/stages-test-engine.yml`. It uses the Linux A10 GPU pool, test-only CUDA NuGet packages,
and the SHA-256-pinned paged Qwen fixture in `foundrylocalmodels/staging/paged-attention`. The existing
`FoundryLocalCore-SP` service connection needs read access to those blobs, and the pipeline needs permission to use
`onnxruntime-Linux-GPU-A10`. These are required resources: do not bypass failures with skips or `continueOnError`.
The job runs directly on the GPU pool's existing image; it does not build or launch a custom container.
The image must provide the compiler and CUDA libraries (CUDA 13 for GPU ORT, CUDA 12 for GenAI, driver 580 or newer).
Missing runtime dependencies fail explicitly. Tests stage their own metadata without modifying the source model.
Missing Engine capability, missing/changed model files, missing lifecycle tests, skipped tests, and test failures all
fail the lane. Its binaries are not published as SDK artifacts and its GenAI pin is independent of the shipping/release
dependency pins.

## Debugging skips in CI

If model-using tests skip unexpectedly in CI, check the `SharedTestEnv: CI detected` banner in stdout — it reports the value of `FOUNDRY_TEST_DATA_DIR`. `(unset; all model-using tests will skip)` means the CI agent didn't mount the shared model cache. A specific `SharedTestEnv: skipping <model> in CI` line means the cache is mounted but that particular model isn't pre-staged.
