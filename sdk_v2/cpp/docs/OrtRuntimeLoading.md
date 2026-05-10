# ORT Runtime Library Loading Strategy

## Problem

`foundry_local.dll` depends on `onnxruntime.dll` and `onnxruntime-genai.dll`. On Windows,
a system-installed `onnxruntime.dll` may exist in the PATH (e.g., from another application
or a system-wide ORT installation). When the OS DLL loader resolves `foundry_local.dll`'s
import table, it may find the system copy first, causing API version issues at runtime.

The C# SDK v2 previously worked around this in `DllLoader.cs` by pre-loading the ORT DLLs
from a known path using `NativeLibrary.TryLoad()` *before* loading `foundry_local.dll`.
That C#-specific pre-loading has been removed — the native code now owns DLL loading
entirely. But the core problem remains: every deployment layout needs ORT DLLs loaded
from the correct directory, in the correct order, before any ORT API call.

## Current Approach: Co-location

The build system copies `onnxruntime.dll` and `onnxruntime-genai.dll` into the same
directory as `foundry_local.dll` via post-build commands in `CMakeLists.txt`. This works
when the consuming binary is in the same directory (tests, examples), because the Windows
loader checks the DLL's own directory first.

**Limitations:**
- Doesn't help when `foundry_local.dll` is deployed separately from ORT (e.g., NuGet
  `runtimes/` layout, or a binding loading it from a different directory).
- Doesn't help when the consuming process has a system `onnxruntime.dll` in its own
  directory or in a PATH-visible location that gets searched before the SDK directory.
- On Linux, co-location works because `IMPORTED_NO_SONAME` prevents RUNPATH issues,
  and `dlopen` with a relative path finds the co-located `.so`.

## Implemented Approach: Delay-Load + C ABI Path Hook

### Design

Uses **delay-load linking** for `onnxruntime.dll` and
`onnxruntime-genai.dll` on Windows, with a C ABI function that lets callers specify
the directory to load them from.

### Why Delay-Load Works Here

ORT is not needed at `foundry_local.dll` load time. The first actual ORT API call happens
during either:
- **EP detection** — `OrtEnv::GetEpDevices()` (touches `onnxruntime.dll` directly)
- **Model load** — `OgaModel::Create()` (touches `onnxruntime-genai.dll`, which itself
  has a load-time dependency on `onnxruntime.dll`)

With delay-load, the DLLs aren't resolved until the first call into them. This creates
a window between "SDK loaded" and "ORT needed" where the caller can specify a path.

### Load Order Contract

```
1. Application loads foundry_local.dll     ← ORT not loaded yet
2. config.SetRuntimeLibraryPath(path)     ← stores path for eager/delay-load resolution
3. Manager_Create(config)                  ← reads path, then EagerLoadOrtDlls():
                                              a) loads onnxruntime.dll from configured path
                                                 (or foundry_local.dll's own directory)
                                              b) loads onnxruntime-genai.dll — its own import
                                                 of onnxruntime.dll resolves to the instance
                                                 loaded in (a) via Windows module dedup
4. EP detection, Model load, etc.          ← ORT DLLs already resident; delay-load hook
                                              acts as a safety net if somehow missed
```

The eager load in step 3 is the primary path. It runs single-threaded during Manager
construction, avoiding races when multiple threads trigger model loads concurrently.
The delay-load hook (`__pfnDliNotifyHook2`) remains as a fallback — if a delay-loaded
function is called before eager loading ran, the hook redirects resolution to the
configured path and enforces the ORT-before-GenAI order.

### Implementation

| Component | Change |
|-----------|--------|
| **CMakeLists.txt** | `/DELAYLOAD:onnxruntime.dll /DELAYLOAD:onnxruntime-genai.dll` linker flags and `delayimp.lib` (Windows only) |
| **`src/util/delay_load_hook_windows.cc`** | `__pfnDliNotifyHook2` implementation that redirects delay-load resolution to configured path (safety net) |
| **`src/util/runtime_library_path.cc`** | Thread-safe path storage with set-once guard |
| **`src/manager.cc`** | `EagerLoadOrtDlls()` — loads ORT then GenAI during Manager construction (single-threaded). Discovers `foundry_local.dll`'s own directory via `GetModuleHandleExW` as the default search path when `RuntimeLibraryPath` isn't set. |
| **Linux** | No delay-load needed. Path is stored but not used yet. Co-location remains the primary strategy. |

### Configuration

Set via `Configuration::SetRuntimeLibraryPath`:
```cpp
auto config = fl::Configuration("myapp")
    .SetRuntimeLibraryPath("/path/to/ort/libs");
```

Or via the C API:
```c
const flApi* api = FoundryLocalGetApi(FL_API_VERSION);
const flConfigurationApi* config_api = api->GetConfigurationApi();
flConfiguration* config;
config_api->Create("myapp", &config);
config_api->SetRuntimeLibraryPath(config, "/path/to/ort/libs");
```

### What This Fixes for All Bindings

With eager loading in the native code, bindings just need to load `foundry_local.dll` —
no per-language DLL pre-loading dance. If ORT DLLs are co-located with `foundry_local.dll`,
everything works with zero configuration. If they're in a different directory, set the path:
- **C#:** `config.RuntimeLibraryPath = dllDir;`
- **Python:** `config.runtime_library_path = path`
- **Rust/JS/etc.:** Call `SetRuntimeLibraryPath` via the C API Configuration setter

### Co-location Remains the Default

The post-build copy commands stay. Co-location is the zero-configuration path for
development and simple deployments. When `RuntimeLibraryPath` is not set, `EagerLoadOrtDlls()`
discovers `foundry_local.dll`'s own directory via `GetModuleHandleExW` and loads ORT DLLs
from there. `RuntimeLibraryPath` only needs to be set when the ORT DLLs are in a different
directory than `foundry_local.dll`.

## History

- **Initial (C++ port):** Load-time linking + co-location. Works for co-located deployments.
- **C# SDK v2:** Added `DllLoader.cs` with `NativeLibrary.TryLoad()` pre-loading as a
  C#-specific workaround for non-co-located deployments.
- **C# commit `5c68c52d`:** Fixed `DllNotFoundException` by checking if ORT is already
  loaded before attempting reload. Managed-specific fix.
- **Implemented:** Delay-load + Configuration-based path. `/DELAYLOAD` linker flags on
  the shared library, `__pfnDliNotifyHook2` in `src/util/delay_load_hook_windows.cc`,
  path set via `Configuration::SetRuntimeLibraryPath()` (formal member, not `additional_options`)
  and applied during Manager construction. Co-location remains default.
- **Eager loading:** Added `EagerLoadOrtDlls()` in `manager.cc` to load both DLLs
  during `Manager::Create()` (single-threaded), avoiding races when concurrent
  `Model::Load()` calls trigger the delay-load hook simultaneously. Discovers
  `foundry_local.dll`'s own directory as the default co-location path. C# `DllLoader`
  ORT pre-loading removed — native code now owns the full DLL loading contract.
