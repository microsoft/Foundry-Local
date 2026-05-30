# sdk_v2 JS pipeline plan

Plan for wiring the `sdk_v2/js` package into the v2 CI pipeline. Companion
to [sdk_v2-pipeline-plan.md](sdk_v2-pipeline-plan.md), which documents the
as-built C++, C#, and Python stages. This doc captures the design decisions
for JS before any YAML is written so we can iterate on the questions.

## Scope

In scope:
- Per-platform build of the Node-API addon (`foundry_local_node.node`,
  `foundry_local_preload.node`) against the matching `cpp-native-<rid>`
  artifact.
- Assembly of a single combined npm tarball (`foundry-local-sdk-*.tgz`)
  containing prebuilds for every supported platform.
- Per-platform integration tests via `vitest run`, gated by
  `FOUNDRY_TEST_DATA_DIR`.

Out of scope (for this change):
- Linux ARM64. ARM64 (Linux and other) is new for sdk_v2 across the board
  and will be added as a separate cross-cutting work item that also covers
  the C++ native build, C#, and Python.
- WinML variant. The base plan doc explicitly scopes JS out of WinML; if
  we want a `foundry-local-sdk-winml` later, it's a copy of the base graph
  consuming `cpp-native-win-*-winml`.
- npm publishing. This change produces the `.tgz` as a pipeline artifact
  only, mirroring how Python/C# produce wheels/nupkgs and leave publish to
  a downstream pipeline.

## Supported platforms (this change)

| Platform        | Native build stage         | JS build | JS test | Notes                                  |
|-----------------|----------------------------|----------|---------|----------------------------------------|
| Windows x64     | `cpp_build_win_x64`        | ✅        | ✅       |                                        |
| Windows ARM64   | `cpp_build_win_arm64`      | ✅        | ❌       | Cross-compiled on x64 host             |
| Linux x64       | `cpp_build_linux_x64`      | ✅        | ✅       |                                        |
| macOS ARM64    | `cpp_build_osx_arm64`      | ✅        | ✅       | Native on AcesShared Sequoia           |

Linux ARM64 will be added in a follow-up alongside the C++/C#/Python
ARM64 work.

## Reference: how v1 ships a multi-platform tarball

v1 ([stages-sdk-v1.yml](../v1/templates/stages-sdk-v1.yml)) does this in
two stages:

1. **`build_js_addon`** — fan-out, one job per (platform, arch). Each
   builds just the Node-API addon, publishes a `js-addon-<plat>-<arch>`
   artifact containing a single `.node` file.
2. **`build_js`** — single job on a Windows agent. Downloads every
   `js-addon-*` artifact, drops each one into
   `sdk/js/prebuilds/<plat>-<arch>/`, runs `npm pack`, publishes one
   combined `js-sdk` `.tgz`.

The published `foundry-local-sdk` package on npmjs.org contains all
platforms because of step 2, not because npm resolves a per-platform
tarball at install time. v2 mirrors this shape.

## Target stage graph

```
compute_version
   │
   ├── cpp_build_win_x64 ──── js_build_win_x64 ─────────────────┐
   ├── cpp_build_win_arm64 ── js_build_win_arm64 (build-only) ──┤
   ├── cpp_build_linux_x64 ── js_build_linux_x64 ───────────────┼── js_pack ──> js-sdk
   ├── cpp_build_osx_arm64 ── js_build_osx_arm64 ───────────────┘
   │                                  │
   │                                  └─> js_test_<rid> (one per build that has tests)
```

* Build stages depend on `compute_version` + the matching
  `cpp_build_<rid>` stage.
* Each build stage publishes `js-prebuild-<rid>` (contents:
  `foundry_local_node.node`, `foundry_local_preload.node`, and the
  matching `foundry_local.{dll,so,dylib}`).
* Test stages depend on their matching build stage and check out
  `test-data-shared`. `win-arm64` is build-only (matches the C# and
  Python matrix).
* `js_pack` depends on all four build stages, downloads every
  `js-prebuild-<rid>`, assembles them under `sdk_v2/js/prebuilds/`,
  stamps the version into `package.json`, runs `npm run build:ts`,
  `npm pack`, and publishes a single `js-sdk` pipeline artifact.

## Per-stage artifacts

| Stage                  | Artifact name              | Contents                                                  |
|------------------------|----------------------------|-----------------------------------------------------------|
| `js_build_win_x64`     | `js-prebuild-win-x64`      | `prebuilds/win32-x64/{foundry_local_node.node, foundry_local_preload.node, foundry_local.dll}` |
| `js_build_win_arm64`   | `js-prebuild-win-arm64`    | `prebuilds/win32-arm64/{…, foundry_local.dll}`             |
| `js_build_linux_x64`   | `js-prebuild-linux-x64`    | `prebuilds/linux-x64/{…, libfoundry_local.so}`             |
| `js_build_osx_arm64`   | `js-prebuild-osx-arm64`    | `prebuilds/darwin-arm64/{…, libfoundry_local.dylib}`       |
| `js_pack`              | `js-sdk`                   | `foundry-local-sdk-<version>.tgz`                          |

Note the directory naming mismatch: ADO RID convention uses
`win-x64`/`osx-arm64`, while the npm prebuild layout uses Node.js
conventions (`win32-x64`/`darwin-arm64`). The artifact name uses the RID
to match the rest of the v2 pipeline; the in-tarball directory uses
Node's `<process.platform>-<process.arch>` to match what the runtime
loader expects.

## Build-time native dependencies

The Node-API addon links against `foundry_local` (import lib on Windows,
shared lib on Linux/macOS) and `#include`s both `sdk_v2/cpp/include/`
and one vcpkg-provided header: `<gsl/span>`, pulled in transitively
from [`foundry_local_cpp.h`](../../sdk_v2/cpp/include/foundry_local/foundry_local_cpp.h).
The public C++ wrapper API targets C++17 (so it can be consumed by
C++17 codebases), which is why `gsl::span` is used instead of
`std::span` (C++20). The pure C ABI (`foundry_local_c.h`) has no
vcpkg dependency.

Approach: **bundle the ms-gsl headers into the existing
`cpp-native-include` artifact** so the C++ consumption payload is
self-contained. ms-gsl is header-only and ~30 KB. Anyone consuming
the published native artifact — JS CI, third-party C++ consumers, or
a future C# native interop layer — gets headers + binaries in one
place with no external dependency lookups.

Concretely:

- [stages-build-native.yml](templates/stages-build-native.yml)'s
  `cpp_build_win_x64` stage is the canonical source of
  `cpp-native-include`. Its `steps-build-windows.yml` invocation gains
  one step that copies `vcpkg_installed/x64-windows/include/gsl/` into
  the include staging directory alongside `foundry_local/`.
- The JS build stages download `cpp-native-<rid>` (binaries +
  `foundry_local.{lib,dll,so,dylib}`) and `cpp-native-include`
  (`foundry_local/` + `gsl/`). `binding.gyp` gets `-I` for both
  subdirectories.
- The gyp helpers ([print-vcpkg-include.mjs](../../sdk_v2/js/script/gyp/print-vcpkg-include.mjs)
  and [print-import-lib-dir.mjs](../../sdk_v2/js/script/gyp/print-import-lib-dir.mjs))
  gain env-var overrides (`FOUNDRY_LOCAL_INCLUDE_DIR`,
  `FOUNDRY_LOCAL_LIB_DIR`) so the JS build stage can point them at the
  downloaded artifact directories without fabricating a fake
  `sdk_v2/cpp/build/...` tree. The overrides are also a real DX win
  for external consumers building the addon themselves.

## Cross-compile: win-arm64

Mirror v1: build on the win-x64 agent, invoke
`node-gyp rebuild --arch=arm64`. `foundry_local.{dll,lib}` from
`cpp-native-win-arm64` and the public+gsl headers from
`cpp-native-include` provide everything the linker needs. ms-gsl is
header-only so the same `cpp-native-include` payload works for every
arch. No tests run for this stage — same as Python and C# win-arm64
matrix entries.

## Test stage details

Mirrors `steps-test-python.yml` patterns:

- `NodeTool@0` for Node 20.
- Check out `test-data-shared` via the shared `checkout-steps.yml`.
- On macOS, `brew install git-lfs` before checkout (same as Python).
- Download the matching `js-prebuild-<rid>` artifact, drop into
  `sdk_v2/js/prebuilds/<plat>-<arch>/`.
- `npm ci`, `npm run build:ts`, `npx vitest run`. v2 `package.json`
  has no `preinstall` / `postinstall` / `prepare` hooks, so no need for
  `--ignore-scripts`.
- Set `FOUNDRY_TEST_DATA_DIR=$(testDataSharedDir)` via the step's
  `env:` (same hand-off pattern as `steps-test-python.yml`).

## Coordinator wire-up

[stages-sdk-v2.yml](templates/stages-sdk-v2.yml) gets one additional
include:

```yaml
- template: stages-js.yml
```

JS doesn't take ORT/GenAI/WinML version parameters — it links
transitively through `foundry_local`, so the native build stages already
have those pins baked in.

## Signing

ESRP signing of native binaries, modeled on the C# pattern in
[steps-build-cs.yml](templates/steps-build-cs.yml). The `.tgz` itself
is **not signed** — npm has no equivalent to NuGet package signing.
Provenance attestation (`npm publish --provenance`) is a publish-time
concern and belongs in a separate release pipeline.

Signing runs in each `js_build_<rid>` stage on the agent that produced
the binaries (signing in `js_pack` would require routing back to a
macOS agent to sign Darwin binaries). After signing, the signed files
are published as `js-prebuild-<rid>`.

| Stage                  | Files signed                                                   | ESRP keyCode  | Tool        |
|------------------------|----------------------------------------------------------------|---------------|-------------|
| `js_build_win_x64`     | `foundry_local_node.node`, `foundry_local_preload.node`, `foundry_local.dll` | `CP-230012`   | SigntoolSign (Authenticode) |
| `js_build_win_arm64`   | same three Windows files                                       | `CP-230012`   | SigntoolSign |
| `js_build_osx_arm64`   | both `.node` files + `libfoundry_local.dylib`                  | (Mac sign keyCode — confirm during implementation; likely `CP-401337` or equivalent) | `codesign` via ESRP |
| `js_build_linux_x64`   | none                                                            | n/a           | Linux `.so` has no standard signing |
| `js_pack`              | none                                                            | n/a           | `.tgz` not signed by npm convention |

The Windows signing block is a near-copy of the SDK DLL signing step in
[steps-build-cs.yml](templates/steps-build-cs.yml#L115) — same
`ConnectedServiceName`, ESRP variable group
(`FoundryLocal-ESRP-Signing`), and `signConfigType: inlineSignParams`
shape. Only `FolderPath` and `Pattern` change.

## Files added / modified

**New (4):**
- `.pipelines/v2/templates/stages-js.yml`
- `.pipelines/v2/templates/steps-build-js.yml`
- `.pipelines/v2/templates/steps-test-js.yml`
- `.pipelines/v2/templates/steps-pack-js.yml`

**Modified (4):**
- `.pipelines/v2/templates/stages-sdk-v2.yml` — emit `stages-js.yml`.
- `.pipelines/v2/templates/steps-build-windows.yml` (x64 path only) —
  add a step that copies `vcpkg_installed/x64-windows/include/gsl/`
  into the `cpp-native-include` staging directory. (Sourced once from
  `cpp_build_win_x64`, same as the existing public-header staging.)
- `sdk_v2/js/script/gyp/print-vcpkg-include.mjs` — honor
  `FOUNDRY_LOCAL_INCLUDE_DIR` env override.
- `sdk_v2/js/script/gyp/print-import-lib-dir.mjs` — honor
  `FOUNDRY_LOCAL_LIB_DIR` env override.

**Unchanged:**
- `sdk_v2/js/script/copy-native.mjs` and
  `sdk_v2/js/script/pack-prebuilds.mjs` — local-dev helpers. CI
  consumes `cpp-native-<rid>` artifacts directly and writes into
  `prebuilds/` without going through these scripts.
- `sdk_v2/cpp/include/foundry_local/foundry_local_cpp.h` —
  `<gsl/span>` stays; the wrapper API remains C++17 for maximum
  consumer compatibility.

## Locked decisions

- **Pack pool:** Windows agent (`onnxruntime-Win-CPU-2022`), matching
  `cpp_pack_nuget`.
- **Install command:** `npm ci --no-audit --no-fund --ignore-scripts`.
  `sdk_v2/js/package-lock.json` is committed (with a negation rule in
  the root `.gitignore` since the repo-wide rule ignores lockfiles for
  v1/samples/www). `--ignore-scripts` is safe because the v2 package
  has no install hooks; the native addon is built explicitly via
  `node-gyp rebuild` later in the same job.
- **Version source:** `$(Pipeline.Workspace)/version-info/sdkVersion.txt`,
  stamped via `npm version <v> --no-git-tag-version --allow-same-version`
  in `js_pack` before `npm pack`.
- **Signing:** Authenticode for Windows binaries, ESRP codesign for
  macOS binaries, none for Linux, none for the `.tgz`. See Signing
  section above.
- **Single combined tarball:** `js_pack` assembles all four prebuilds
  and produces one `foundry-local-sdk-<version>.tgz`. Matches v1.
- **JS scoped out of WinML matrix.** Locked by the base plan doc.
- **Linux ARM64 deferred** to the cross-cutting ARM64 work item.

## Deferred / measure-don't-prejudge

- **Vitest concurrency on shared pools.** Some JS tests load real
  models (gated by `FOUNDRY_TEST_DATA_DIR`). On macOS especially the
  AcesShared agents have limited RAM. We may need
  `vitest run --pool=forks --poolOptions.forks.singleFork=true` to
  serialize. Measure on first run rather than over-engineer up front.
- **ESRP macOS signing keyCode.** Need to confirm the exact keyCode +
  `operationSetCode` combination during implementation by checking the
  ESRP service config. Falls back cleanly to skipping macOS signing if
  the policy isn't provisioned yet.

## Stage dependency summary (additions only)

```
compute_version ──┐
                  │
cpp_build_win_x64 ─────────────> js_build_win_x64    ──┬── js_test_win_x64
cpp_build_win_arm64 ──────────── js_build_win_arm64    │
cpp_build_linux_x64 ──────────── js_build_linux_x64  ──┼── js_test_linux_x64
cpp_build_osx_arm64 ──────────── js_build_osx_arm64  ──┼── js_test_osx_arm64
                                                       │
                                                  (all 4) ──> js_pack ──> js-sdk artifact
```
