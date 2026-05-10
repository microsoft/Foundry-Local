# CI Build Plan — sdk_v2/cpp

Status: **proposed** (pipeline scaffolded, not yet onboarded in ADO)

This document captures the plan for the first CI pipeline dedicated to the
new C++ SDK (`sdk_v2/cpp`). It explains what we're building, why we made the
choices we did, and what was deliberately deferred.

## Goal

Stand up a single ADO pipeline that, on every change to `sdk_v2/cpp`:

1. Builds the C++ SDK on every supported platform.
2. Runs the test suite where the host can execute the binaries.
3. On `main` only, packs all native binaries into a single
   `Microsoft.AI.Foundry.Local.Runtime` NuGet package.

Start as simple as possible. Layer signing, publishing, and additional SKUs
on top once the basic flow is green.

## Supported Platforms (initial)

| Platform        | Pool                              | Build | Test | Notes                                   |
|-----------------|-----------------------------------|-------|------|-----------------------------------------|
| Windows x64     | `onnxruntime-Win-CPU-2022`        | ✅    | ✅   | Also stages public headers              |
| Windows ARM64   | `onnxruntime-Win-CPU-2022`        | ✅    | ❌   | Cross-compiled from x64 host            |
| Linux x64       | `onnxruntime-Ubuntu2404-AMD-CPU`  | ✅    | ✅   | Pulls extra `OnnxRuntime.Gpu.Linux` pkg |
| macOS ARM64     | `AcesShared` (Sequoia)            | ✅    | ✅   | Native; not available in OneBuild       |

The macOS build is the piece the internal OneBranch pipeline could not do —
OneBranch has no macOS pool type. The repo's existing
[foundry-local-packaging.yml](../../../.pipelines/foundry-local-packaging.yml)
already proves the `AcesShared` pool works for this org, so we reuse it.

## Pipeline Layout

All files live under `.pipelines/sdk_v2/` to keep the new pipeline isolated
from the existing `foundry-local-packaging.yml` while it stabilizes.

```
.pipelines/sdk_v2/
├── foundry-local-native.yml          # Entry point, extends 1ES template
└── templates/
    ├── stages-build-native.yml       # 4 build stages + 1 pack stage
    ├── steps-prefetch-nuget.yml      # ORT/GenAI/WinML NuGet pre-fetch (pwsh + bash)
    ├── steps-build-windows.yml       # arch: x64 | arm64
    ├── steps-build-linux.yml
    ├── steps-build-macos.yml
    └── steps-pack-nuget.yml          # Runs sdk_v2/cpp/nuget/pack.py
```

The repo-shared `.pipelines/templates/checkout-steps.yml` is reused as-is to
clone `test-data-shared` (LFS) via the `FoundryLocalCore-SP` Azure CLI
service connection. No need to re-implement LFS auth.

## Stages

```
compute_version ─┐
                 ├──► build_win_x64    ─┐
                 ├──► build_win_arm64  ─┤
                 ├──► build_linux_x64  ─┼──► pack_nuget   (main only)
                 └──► build_osx_arm64  ─┘
```

* The four build stages are independent (`dependsOn: []`) and run in parallel.
* `pack_nuget` depends on all five and is gated:
  `condition: and(succeeded(), eq(variables['Build.SourceBranch'], 'refs/heads/main'))`.
  Pull requests run all four builds but skip packing.

## Per-stage Artifacts

Published via 1ES `templateContext.outputs` (no manual `PublishPipelineArtifact`):

| Stage             | Artifact name        | Contents                                          |
|-------------------|----------------------|---------------------------------------------------|
| `compute_version` | `version-info`       | `sdkVersion.txt`, `pyVersion.txt`, `flcVersion.txt` |
| `build_win_x64`   | `native-win-x64`     | `foundry_local.dll`, `foundry_local.pdb`          |
| `build_win_x64`   | `native-include`     | Public headers (sourced once, from win-x64)       |
| `build_win_arm64` | `native-win-arm64`   | `foundry_local.dll`, `foundry_local.pdb`          |
| `build_linux_x64` | `native-linux-x64`   | `libfoundry_local.so`                             |
| `build_osx_arm64` | `native-osx-arm64`   | `libfoundry_local.dylib`                          |
| `pack_nuget`      | `nuget`              | `Microsoft.AI.Foundry.Local.Runtime.<version>.nupkg` |

## Versioning

Copied verbatim from the existing repo pipeline's `compute_version` stage so
the convention is consistent across pipelines:

* `sdkVersion` — semver for NuGet/JS/C#/Rust  (e.g. `0.1.0-dev.202605111234`)
* `pyVersion`  — PEP 440 for Python           (e.g. `0.1.0.dev202605111234`)
* `flcVersion` — Core/native-style            (e.g. `0.1.0-dev-202605111234-abc12345`)

The pack stage consumes `sdkVersion.txt`. The other two are produced for
future use (Python wheel, Core publishing) so we don't have to change the
version stage when we extend the pipeline.

Pipeline parameters allow override:

* `version`        — base version, default `0.1.0`
* `prereleaseId`   — `none` (default) or any string (`rc1`, `beta`, …)
* `isRelease`      — boolean, drops the dev/timestamp suffix
* `buildConfig`    — CMake config, default `RelWithDebInfo`

## Dependency Versions

The pipeline pins ORT / GenAI / WinML versions and downloads the NuGet
packages from the **aiinfra ADO feed** before invoking CMake. This serves
two purposes:

1. **Network reliability** — the public NuGet feed has rate-limit / outage
   issues we've hit in CI before.
2. **Version pinning** — the `KEY=PATH` pairs are passed via
   `--cmake_extra_defines` (`ORT_FETCH_URL`, `GENAI_FETCH_URL`,
   `WINML_EP_CATALOG_FETCH_URL`, `ORT_GPU_LINUX_FETCH_URL`) so the cmake
   defaults in `FindOnnxRuntime.cmake` / `FindOnnxRuntimeGenAI.cmake` are
   never silently substituted.

Versions are pipeline-level variables, currently:

* `ortVersion`    `1.24.4`
* `genaiVersion`  `0.13.1`
* `winmlVersion`  `1.8.2091`

These must be kept in sync with the cmake defaults. When bumping, update
both places in the same PR.

The shared download logic is in `steps-prefetch-nuget.yml` and exposes both
a PowerShell (Windows) and a bash (Linux/macOS) implementation behind a
`shell` parameter. It emits a single pipeline variable
`cmakeFetchDefines` containing the quoted `KEY=PATH` pairs to splice into
the build command.

## Test Data

Tests pull `test-data-shared` (LFS) via the existing
`.pipelines/templates/checkout-steps.yml` template. This is already wired to
the `FoundryLocalCore-SP` service connection and is the same auth path used
by the existing pipeline, so there is no new credential to provision.

Test data is fetched on:
* `build_win_x64`   (tests run on x64 only)
* `build_linux_x64`
* `build_osx_arm64`

Skipped on `build_win_arm64` (cross-compile, no test execution on the host).

## Build Commands

All platforms invoke the standard build driver (no out-of-band CMake calls):

```bash
python build.py --configure --build [--test] --config <buildConfig> \
                --cmake_extra_defines $(cmakeFetchDefines)
```

Windows x64 explicitly passes `--cmake_generator "Visual Studio 17 2022"`.
Windows ARM64 adds `--arm64`. macOS expects `cmake`/`ninja`/`pkg-config` and
falls back to Homebrew if any are missing.

The build output directories follow `build.py`'s convention:
`sdk_v2/cpp/build/<Windows|Linux|macOS>/<Config>/...`.

## Triggers

* `pr: [main, releases/*]` — all four builds run, pack is skipped.
* No CI trigger on push for the initial rollout. Manual queues against
  `main` will exercise the pack stage.

We can switch to a CI trigger on `main` once we trust the pipeline.

## What's Deliberately Deferred

* **ESRP signing.** Slot a signing stage between the builds and pack once
  the unsigned end-to-end is green.
* **Publishing.** No push to NuGet/internal feeds in this pipeline.
* **Linux ARM64 / macOS x64.** Add when there's a customer ask; pools and
  pack support are both ready.
* **Code coverage upload.** `run_coverage.ps1` exists for local use but is
  not wired into CI.
* **WinML SKU variants.** The existing `foundry-local-packaging.yml`
  produces both standard and WinML packages; the new pipeline produces one
  package. Add a WinML SKU when needed.

## Reference: Internal Pipeline

The internal repo's `D:\src\github\fl.sdk.internal\.pipelines\sdk_v2`
pipeline implements the same overall flow on OneBranch. It was the
starting point for this work, with the following deliberate divergences:

* **1ES instead of OneBranch.** The repo already uses 1ES; OneBranch's
  containerized Linux + blocked public network was the source of every
  workaround in the internal pipeline.
* **macOS ARM64 build added.** OneBranch had no macOS pool type.
* **Reuses repo-shared pools and templates** (`onnxruntime-Win-CPU-2022`,
  `checkout-steps.yml`).
* **Single nupkg per build**; the internal pipeline's separation of stages
  is preserved but condensed.

## Files

* Entry: [.pipelines/sdk_v2/foundry-local-native.yml](../../../.pipelines/sdk_v2/foundry-local-native.yml)
* Stages: [.pipelines/sdk_v2/templates/stages-build-native.yml](../../../.pipelines/sdk_v2/templates/stages-build-native.yml)
* Windows steps: [.pipelines/sdk_v2/templates/steps-build-windows.yml](../../../.pipelines/sdk_v2/templates/steps-build-windows.yml)
* Linux steps: [.pipelines/sdk_v2/templates/steps-build-linux.yml](../../../.pipelines/sdk_v2/templates/steps-build-linux.yml)
* macOS steps: [.pipelines/sdk_v2/templates/steps-build-macos.yml](../../../.pipelines/sdk_v2/templates/steps-build-macos.yml)
* Pre-fetch: [.pipelines/sdk_v2/templates/steps-prefetch-nuget.yml](../../../.pipelines/sdk_v2/templates/steps-prefetch-nuget.yml)
* Pack: [.pipelines/sdk_v2/templates/steps-pack-nuget.yml](../../../.pipelines/sdk_v2/templates/steps-pack-nuget.yml)
* Pack tool: [sdk_v2/cpp/nuget/pack.py](../nuget/pack.py)
