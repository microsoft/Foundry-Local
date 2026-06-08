---
description: Use when touching ORT/GenAI loading, DLL/SO discovery, the `foundry_local` native library's link dependencies, or any binding's native-load logic (C# `DllLoader`, Python `lib_loader`, etc.).
applyTo: "sdk_v2/{cpp/src/util,cpp/src/manager.cc,cs/src/Detail/DllLoader.cs,python/src/foundry_local_sdk/_native}/**"
---

# ORT Runtime Loading Contract

The native `foundry_local` library has **no ORT-loading machinery**: no
`/DELAYLOAD` flags, no `RuntimeLibraryPath` Configuration knob, no
`EagerLoadOrtDlls()`, no delay-load hook. `onnxruntime` and `onnxruntime-genai`
are ordinary load-time link dependencies on every platform.

Every language binding is responsible for preloading ORT then GenAI by absolute
path before loading `foundry_local`. See
[`sdk_v2/cpp/docs/OrtRuntimeLoading.md`](../../sdk_v2/cpp/docs/OrtRuntimeLoading.md)
for the full contract.

## Rules

- **Do not** re-add `/DELAYLOAD:onnxruntime.dll` or `/DELAYLOAD:onnxruntime-genai.dll` to `CMakeLists.txt`.
- **Do not** re-add a `RuntimeLibraryPath` field to `Configuration` (C++, C#, or Python). The knob was deliberately removed because bindings own ORT discovery; consumers needing a custom path do platform-native preload themselves.
- **Do not** add a delay-load notify hook (`__pfnDliNotifyHook2`) or any `LoadLibrary`/`dlopen` of ORT/GenAI inside the native library. By the time native code runs, the loader has already resolved (or failed to resolve) those deps.
- **Do** add the binding-side preload sequence (ORT → GenAI → foundry_local, all by absolute path) when bringing up a new language binding. Mirror C# `PreloadOrtIfPresent` in `DllLoader.cs` or Python `prepare_native_dependencies` in `lib_loader.py`.
- **Do** keep the post-build copy commands that co-locate ORT/GenAI next to `foundry_local` in the build tree — tests and examples depend on the OS default search finding them via the binary's own directory.

## Why

- POSIX has no portable equivalent of MSVC `/DELAYLOAD` (`DT_NEEDED` is always eager). A native-side loader can only work on Windows.
- The OS loader's loaded-module table is keyed by SONAME (POSIX) / base name (Windows). Once the binding loads ORT by absolute path, the native lib's `DT_NEEDED` / import-table entries resolve to the resident instance — same mechanism on all three platforms.
- This sidesteps SONAME-mismatch issues (`libonnxruntime.so` vs. `libonnxruntime.so.1`) and PATH-capture issues (system `onnxruntime.dll` winning over the SDK copy).
