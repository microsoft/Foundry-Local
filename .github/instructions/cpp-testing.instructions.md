---
description: "Use when writing or modifying C++ integration tests, test helpers, SharedTestEnv, model fixtures, or debugging test failures involving model selection or variant filtering."
applyTo: "sdk_v2/cpp/test/**"
---
# C++ Testing

## Variant Selection in Test Helpers

`GetModels()` returns only the *selected* variant per alias group. The catalog's default sort prefers GPU variants. If a test needs a CPU-only model (e.g., embeddings), the CPU variant is invisible unless you use `GetVariants()`.

Test helpers that filter by device type **must**:
1. Iterate `GetVariants()` on each alias group
2. Call `SelectVariant()` when the currently-selected variant doesn't match the desired device/task
3. Never assume the selected variant matches the filter criteria

See `FindSmallestModelByTask`, `FindSmallestModelByName`, `FindSmallestToolCallingModel` in `shared_test_env.h` for the pattern.

## SharedTestEnv Model Loading

`SharedTestEnv` loads models once for all integration tests. The setup phase:
1. Populates catalog from Azure
2. Downloads CUDA EP (~90% → 100% progress)
3. Loads chat, tool-calling, audio, streaming-audio, and embeddings models
4. Prints `SharedTestEnv: <role>=<model-name>` for each

If a model role prints `(none)`, the test fixtures for that role will skip. Check variant selection logic first — the model likely exists but the wrong variant is selected.

## Test Binary Quick Reference

| Binary | Type | Source dir | Startup | Count |
|--------|------|-----------|---------|-------|
| `foundry_local_tests.exe` | Unit / internal API | `test/internal_api/` (+ pure-logic `test/sdk_api/`) | Fast (no models) | ~700 |
| `sdk_integration_tests.exe` | Integration / public API | `test/sdk_api/` | Slow (model loading, real web service) | ~100 |
| `cache_only_tests.exe` | Integration / public API (cache-only + external service URL) | `test/sdk_api/` | Moderate (own `Manager` configs) | ~10 |
| `external_mode_tests.exe` | Integration / public API (client/server split) | `test/sdk_api/` | Slow (spawns a `--serve` host child, loads a model) | 2 |

`cache_only_tests` and `external_mode_tests` live in `test/sdk_api/` but build into their own binaries because `Manager` is a singleton — each needs `Manager` instances configured differently than the shared `SharedTestEnv` host, so they cannot coexist in `sdk_integration_tests`' process. Source location tracks the API surface (public, black-box); the binary split is an orthogonal CMake concern.

Always use `--gtest_filter` when iterating on a specific test or small group. The full integration suite takes ~10 minutes.

## Real integration tests are mandatory for public-API surface

Any new behavior reachable through the public SDK API — `Model::Load`/`Unload`/`IsLoaded`, `Session`/`ChatSession`, catalog operations, the web service endpoints, etc. — **must** get a "real" integration test under `test/sdk_api/` (the `sdk_integration_tests` binary) that drives the *production entry point* end-to-end against the real backend (real cached model and/or a real in-process `WebService`). An `internal_api` unit test that exercises an internal class directly, or stubs the other side of a contract, is **not** a substitute and does not satisfy this requirement.

Rules:
- When a feature spans a client/server or local/external boundary that both live in this repo, write a **round-trip test that goes through the real server**, not a stub. Stubbed boundary tests are fine *in addition*, never *instead*.
- Drive the same entry point the user calls (`Model::Load()`, not the internal router/manager) so the test exercises identifier translation, serialization, and endpoint resolution exactly as production does.
- Use realistic identifiers. If production passes a model_id, the test must pass a model_id — not an alias that happens to resolve.

## Inner-loop workflow for a single test

When verifying a fix or a new test, never re-run the whole suite via `ctest`. The fast path is:

1. **Incremental build only** (skip configure and the full test phase):
   ```
   python build.py --build --config RelWithDebInfo
   ```
2. **Run the specific test binary directly with `--gtest_filter`**:
   ```
   cd sdk_v2\cpp\build\Windows\RelWithDebInfo\bin\RelWithDebInfo
   .\foundry_local_tests.exe   --gtest_filter="MySuite.MyTest"
   .\sdk_integration_tests.exe --gtest_filter="ModelFixture.TestName"
   ```

Filter syntax: `Suite.Test`, wildcards (`Suite.*`), or colon-separated lists (`Test1:Test2:Test3`). This avoids the ~10-minute model load in `sdk_integration_tests` when iterating on an unrelated test.
