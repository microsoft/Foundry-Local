# sdk_v2 C# and Python pipeline plan

Plan of record for replicating the legacy C# and Python build/test/pack flow
for the new `sdk_v2/` SDKs, where the C++ SDK in `sdk_v2/cpp` is the native
runtime dependency (replacing the legacy `foundry-local-core` from
neutron-server).

## Scope

Source projects: `sdk_v2/cs/src` and `sdk_v2/python`. Native dependency: the
C++ SDK in `sdk_v2/cpp`, already built by `.pipelines/sdk_v2/foundry-local-native.yml`
and packaged as `Microsoft.AI.Foundry.Local.Runtime[.WinML]`.

Out of scope: JS, Rust, and a standalone `foundry-local-runtime` Python wheel.

## Architectural decisions (locked)

1. **C# Runtime version pinning.** SDK csproj is built with
   `/p:FoundryLocalRuntimeVersion=$(sdkVersion)` where `sdkVersion` is the
   same string used to pack the Runtime nupkg. SDK and Runtime always ship
   as a matched pair.
2. **C# WinML SKU.** `UseWinML=true` flips the `PackageReference` to
   `Microsoft.AI.Foundry.Local.Runtime.WinML`. One small csproj edit splits
   the existing single-package block into two `UseWinML`-conditional blocks.
3. **Python SDK wheel bundles `foundry_local` directly** at
   `_native/<rid>/foundry_local.{ext}`. No separate `foundry-local-runtime`
   wheel. `lib_loader.py` already supports this layout. Trade-off accepted:
   SDK and runtime upgrade together. Non-SDK consumers get native libs from
   the NuGet.

   ORT and GenAI native libraries are **not** bundled in the wheel; they are
   resolved at install time from public PyPI (see decision 9). The wheel
   only contains `foundry_local` plus its non-ORT runtime closure.
4. **Test stages set `TEST_MODEL_CACHE_DIR`** to the checked-out
   `test-data-shared` path for all three languages (C++ already does this;
   Python already reads it; C# gets a small `Utils.cs` change to honor it).
5. **Parameter naming.** New templates use `flNugetDir` (not `flcNugetDir`).
   `flc` was Foundry Local Core, which we are replacing.
6. **Shared `compute_version` stage.** Extracted into
   `templates/stages-version.yml` so both `foundry-local-sdk.yml` (new
   top-level) and `foundry-local-packaging.yml` (existing top-level)
   include it once. No duplication.
7. **Python WinML wheel name via custom PEP 517 backend.** The build
   backend at `sdk_v2/python/_build_backend/__init__.py` wraps
   `setuptools.build_meta` and rewrites the project name to
   `foundry-local-sdk-winml` when `FL_PYTHON_PACKAGE_NAME` is set in the
   environment. The same backend also handles ORT pin rewriting (decision 8).
8. **Single source of truth for ORT/GenAI versions.** ORT and GenAI versions
   live in `sdk_v2/deps_versions.json` (standard) and
   `sdk_v2/deps_versions_winml.json` (WinML). Both files have shape
   `{ "onnxruntime": { "version": "..." }, "onnxruntime-genai": { "version": "..." } }`.
   Consumers:
   - **C++ build:** `sdk_v2/cpp/cmake/Find{OnnxRuntime,OnnxRuntimeGenAI}.cmake`
     read versions via `file(READ)` + `string(JSON ... GET ...)`. The
     `if(NOT ORT_VERSION)` guard is preserved so `-DORT_VERSION=...`
     overrides still work.
   - **Python wheel build:** `pyproject.toml` declares ORT pins with
     sentinel `==0.0.0`; the build backend rewrites the pins from JSON
     at wheel-build time. If the backend is ever bypassed, `pip install`
     fails fast with "no matching version" (intentional loud failure).

   Bumping ORT/GenAI is a one-file edit per variant.
9. **ORT/GenAI come from public PyPI.** No private feed plumbing required
   for the wheel install path:
   - `onnxruntime-core` (Windows/macOS, standard + WinML variants)
   - `onnxruntime-genai-core` (Windows/macOS)
   - `onnxruntime-gpu` / `onnxruntime-genai-cuda` (Linux)

   Import-name mapping is platform-dependent: Linux uses `onnxruntime` /
   `onnxruntime_genai`; Windows/macOS use `onnxruntime_core` /
   `onnxruntime_genai_core`.
10. **C++ staging step is the policy authority for native payload contents.**
    `steps-build-{windows,linux,macos}.yml` stage the **full runtime closure**
    of `foundry_local` into the `cpp-native-<rid>` artifact, with explicit
    inclusions/exclusions. Downstream consumers (Python wheel build, C# pack)
    copy the artifact verbatim — they do not re-filter. This keeps the
    "what ships next to `foundry_local`" decision in exactly one place.

    Staging includes all `*.dll`/`*.pdb` (Windows), `*.so`/`*.so.*`/`lib*.so*`
    (Linux), `*.dylib` (macOS) from the build output bin directory and
    excludes:
    - ORT/GenAI (`onnxruntime*`, `Microsoft.Windows.AI.MachineLearning.*`) —
      provided by pip on the Python side and by NuGet on the C# side.
    - Test/example binaries (`*_tests.*`, `*_example.*`, `gtest*`,
      `cmake_test_discovery_*`).

    The step fails loudly if `foundry_local` itself is missing.
11. **Python runtime ORT discovery.** `lib_loader.py::prepare_native_dependencies()`
    bridges between the in-wheel `foundry_local` and the pip-installed ORT
    packages:
    - **Windows:** `os.add_dll_directory(...)` for each ORT package directory.
    - **Linux/macOS:** create symlinks
      `_native/<rid>/{onnxruntime,onnxruntime-genai}.{so,dylib}` pointing at
      the package-installed `lib*` files (workaround for
      [onnxruntime#27263](https://github.com/microsoft/onnxruntime/issues/27263)).

    Wired into `_native/api.py` between `find_library()` and the cffi
    extension import. Idempotent and silent on failure.
12. **`foundry-local-install` CLI.** Declared via `[project.scripts]` in
    `pyproject.toml`, backed by
    `sdk_v2/python/src/foundry_local_sdk/_native/installer.py`. Flags:
    `--winml`, `--verbose`. Verifies installed packages via
    `importlib.util.find_spec` to avoid triggering DLL load during
    verification.

## File layout

### New under `.pipelines/sdk_v2/templates/`

| File | Purpose |
|---|---|
| `stages-version.yml` | Extracted `compute_version` stage. Writes `sdkVersion.txt`, `pyVersion.txt`, `flcVersion.txt` to the `version-info` artifact. |
| `steps-build-cs.yml` | Inner steps: write NuGet.config with local feed for `flNugetDir`, restore + build + ESRP-sign DLLs + pack + ESRP-sign nupkg. Parameters: `flNugetDir`, `isWinML`, `outputDir`. |
| `steps-test-cs.yml` | Inner steps: write NuGet.config, restore + build + run tests. Pass `/p:FoundryLocalRuntimeVersion=$(sdkVersion)` and explicitly empty `/p:FoundryLocalNativeBinDir=` so the dev auto-detect doesn't fire. Set `TEST_MODEL_CACHE_DIR=$(Agent.BuildDirectory)/test-data-shared`. |
| `stages-cs.yml` | Build + test stages, parameterized by `variant: base|winml`. Stages: `cs_build_<variant>`, `cs_test_win_x64[_winml]`, `cs_test_linux_x64`, `cs_test_osx_arm64`. Test stages check out `test-data-shared` and depend on the matching `pack_nuget[_winml]` stage. |
| `steps-build-python.yml` | Inner steps: pass-through copy of the downloaded `cpp-native-<rid>` artifact into `src/foundry_local_sdk/_native/<rid>/` (broad patterns `*.dll`, `*.so`, `*.so.*`, `*.dylib`, `*.pdb` with dedupe). Stamp `version.py` from `pyVersion.txt`, install build deps, `python -m build --wheel`. WinML variant sets `FL_PYTHON_PACKAGE_NAME=foundry-local-sdk-winml`. Filtering policy lives in the C++ staging step (decision 10). |
| `steps-test-python.yml` | Inner steps: install built SDK wheel + ORT/GenAI deps, set `TEST_MODEL_CACHE_DIR`, `pytest sdk_v2/python/test`. |
| `stages-python.yml` | Per-platform build + test stages, parameterized by `variant: base|winml`. Stages: `py_build_<variant>_<rid>`, `py_test_<variant>_<rid>`. |

### New top-level pipeline

`.pipelines/sdk_v2/foundry-local-sdk.yml` composes:

1. `templates/stages-version.yml` (compute_version).
2. `templates/stages-build-native.yml` (existing — produces native artifacts
   + Runtime nupkg + cpp-native-include).
3. `templates/stages-cs.yml` × 2 (variants `base` and `winml`).
4. `templates/stages-python.yml` × 2 (variants `base` and `winml`).

### New under `sdk_v2/`

- `sdk_v2/deps_versions.json`, `sdk_v2/deps_versions_winml.json` — single
  source of truth for ORT/GenAI versions (decision 8).
- `sdk_v2/python/_build_backend/__init__.py` — custom PEP 517 backend
  wrapping `setuptools.build_meta` (decisions 7, 8).
- `sdk_v2/python/src/foundry_local_sdk/_native/installer.py` —
  `foundry-local-install` CLI (decision 12).

### Edits to existing files

- `sdk_v2/cpp/cmake/FindOnnxRuntime.cmake` and `FindOnnxRuntimeGenAI.cmake` —
  read versions from JSON (decision 8).
- `sdk_v2/python/pyproject.toml` — declare `[build-system]` pointing at
  `_build_backend`; ORT pins as sentinel `==0.0.0`; `[project.scripts]`
  entry for `foundry-local-install`.
- `sdk_v2/python/src/foundry_local_sdk/_native/lib_loader.py` — add
  `prepare_native_dependencies()` (decision 11).
- `sdk_v2/python/src/foundry_local_sdk/_native/api.py` — call
  `prepare_native_dependencies()` before cffi extension import.
- `sdk_v2/cs/src/Microsoft.AI.Foundry.Local.csproj` — split the existing
  `PackageReference Microsoft.AI.Foundry.Local.Runtime` block into two
  `UseWinML`-conditional blocks (one for `.Runtime`, one for `.Runtime.WinML`).
- `sdk_v2/cs/test/FoundryLocal.Tests/Utils.cs` — prefer
  `TEST_MODEL_CACHE_DIR` env var when set; current appsettings logic stays
  as fallback.
- `.pipelines/sdk_v2/foundry-local-native.yml` — replace the inline
  `compute_version` stage with `templates/stages-version.yml` include.
- `.pipelines/sdk_v2/templates/steps-build-{windows,linux,macos}.yml` —
  staging step now copies the full runtime closure with
  include/exclude filtering (decision 10).
- `.pipelines/foundry-local-packaging.yml` — after the existing
  `sdk_v2/templates/stages-build-native.yml` extension, add four template
  references for the new sdk_v2 cs/python stages (variants base + winml each).
  The existing top-level `compute_version` stage stays — sdk_v2 stages just
  depend on the same artifact name.

## Stage dependency graph (sdk_v2 only)

```
compute_version
   |
   |-- cpp_build_win_x64 ----------+
   |-- cpp_build_win_arm64 --------|
   |-- cpp_build_linux_x64 --------+--> pack_nuget --+--> cs_build_base --+--> cs_test_win_x64
   |-- cpp_build_osx_arm64 --------+                 |                    |--> cs_test_linux_x64
   |                                                 |                    +--> cs_test_osx_arm64
   |                                                 +--> (cs-sdk-base artifact)
   |
   |   +--> py_build_base_win_x64    --> py_test_base_win_x64
   +-->|--> py_build_base_linux_x64  --> py_test_base_linux_x64
   |   +--> py_build_base_osx_arm64  --> py_test_base_osx_arm64
   |
   |-- cpp_build_win_x64_winml ---+
   |-- cpp_build_win_arm64_winml -+--> pack_nuget_winml --> cs_build_winml --> cs_test_win_x64_winml
   |                              |
   |                              +--> py_build_winml_win_x64 --> py_test_winml_win_x64
   |                                   py_build_winml_win_arm64  (no test — cross-compile)
```

## Per-phase work order

### Phase 1 — C# (delegated to `@CSharpCoder`)

1. Extract `templates/stages-version.yml`; update `foundry-local-native.yml`
   to include it.
2. csproj `UseWinML`-conditional `PackageReference` split.
3. `Utils.cs` `TEST_MODEL_CACHE_DIR` env-var support.
4. `templates/steps-build-cs.yml` and `templates/steps-test-cs.yml`.
5. `templates/stages-cs.yml` (variant-parameterized).
6. New `foundry-local-sdk.yml` composing version + native + cs.
7. Wire cs stages into `foundry-local-packaging.yml`.
8. **Local validation gate** before merge:
   - Build sdk_v2 C++ DLL locally.
   - `python sdk_v2/cpp/nuget/pack.py` to produce a local Runtime nupkg.
   - `dotnet pack /p:FoundryLocalRuntimeVersion=<version> /p:UseWinML=false`
     against that local feed; verify produced nupkg's dependency graph
     lists exactly Runtime + ORT.Foundry + ORT.GenAI.Foundry.
   - Same with `/p:UseWinML=true`.
   - `dotnet test` against the produced package on the Windows host.
9. `@Reviewer` pass before final commit.

### Phase 2 — Python (delegated to `@PythonCoder`)

1. Add `sdk_v2/deps_versions[_winml].json` (decision 8).
2. Custom PEP 517 backend (`_build_backend/__init__.py`) handling both name
   override (WinML) and ORT pin rewrite (decisions 7, 8).
3. `pyproject.toml`: build-system pointer, sentinel ORT pins, scripts entry.
4. `lib_loader.py::prepare_native_dependencies()` + wire into `api.py`
   (decision 11).
5. `installer.py` CLI (decision 12).
6. C++ staging update — `steps-build-{windows,linux,macos}.yml` stage the
   full runtime closure with include/exclude filtering (decision 10).
   Validate locally that the staged set contains `foundry_local` + vcpkg
   shared deps and excludes ORT, WinML, tests, and examples.
7. `templates/steps-build-python.yml` (pass-through copy from artifact) and
   `templates/steps-test-python.yml`.
8. `templates/stages-python.yml` (variant-parameterized).
9. Add python stages to `foundry-local-sdk.yml` and
   `foundry-local-packaging.yml`.
10. **Local validation gate** before merge:
    - Build sdk_v2 C++ DLL locally; verify staging produces the expected
      file set (~14 files on Windows: `foundry_local.{dll,pdb}` + ~12 vcpkg
      DLLs; no ORT/GenAI/WinML; no test/example binaries).
    - `python -m build --wheel`; verify wheel contains the native payload
      under `_native/<rid>/`.
    - `pip install` into a clean venv; confirm `onnxruntime-core` and
      `onnxruntime-genai-core` resolve from PyPI; `import foundry_local_sdk`
      succeeds end-to-end.
11. `@Reviewer` pass.

## Test-data conventions

All three SDKs honor `TEST_MODEL_CACHE_DIR` (env var) pointing at a
checked-out `test-data-shared` working tree:

- **C++** — `steps-build-{windows,linux,macos}.yml` already set
  `TEST_MODEL_CACHE_DIR=$(Agent.BuildDirectory)/test-data-shared` and check
  out `test-data-shared`.
- **Python** — `sdk_v2/python/test/conftest.py` already reads the env var.
- **C#** — `Utils.cs` will be updated to prefer the env var, falling back
  to the existing `appsettings.Test.json` `TestModelCacheDirName` lookup
  for VS / inner-loop usage.

New CI test stages check out `test-data-shared` (LFS) and set
`TEST_MODEL_CACHE_DIR` accordingly. SDK *build* stages do not need
test-data-shared.

## Things explicitly out of scope

- No `foundry-local-runtime` standalone wheel.
- No JS or Rust sdk_v2 stages (only C# and Python were requested).
- No multi-repo `Foundry-Local`/`test-data-shared` path-juggling logic in
  the new templates — sdk_v2 paths are repo-relative.
- No CI signing config changes — reuse the existing ESRP service connection
  and key codes from `.pipelines/templates/build-cs-steps.yml`.
- No private Azure DevOps feed dependency for the Python wheel install
  path — ORT/GenAI come from public PyPI (decision 9).

## Future work (non-blocking)

- **Static linkage of vcpkg deps.** Bundling currently adds ~12 DLLs /
  ~14 MB to each Windows wheel. Static-linking the easy candidates
  (spdlog, fmt) and discussing the harder ones (openssl, curl, oatpp)
  would shrink the wheel and remove the "transitive vcpkg dep silently
  breaks the wheel" risk class.
- **Local-dev `_native/<rid>/` cleanup.** `build.py` populates the in-tree
  staging dir but doesn't prune stale ORT DLLs from prior runs. Pipeline
  is unaffected; only impacts inner-loop devs.
- **Linux/macOS auditwheel/delocate.** Not needed today (the only non-system
  shared deps are the bundled vcpkg libs, which already sit beside
  `libfoundry_local`). Revisit if `manylinux` compliance becomes a
  publishing gate.
