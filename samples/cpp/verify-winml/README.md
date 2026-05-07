# Verify WinML 2.0 Execution Providers (C++)

This sample verifies that WinML 2.0 execution providers are correctly
discovered, downloaded, and registered using the Foundry Local C++ SDK. It then
uses registered WinML EP-backed model variants and finishes with one native
streaming chat check.

## Prerequisites

- Windows with a compatible GPU or NPU
- Visual Studio with C++ build tools
- vcpkg

The CMake build downloads the native NuGet artifacts pinned by the SDK and
copies the required WinML runtime DLL (`Microsoft.Windows.AI.MachineLearning.dll`)
next to `VerifyWinML.exe` automatically. Set
`FOUNDRY_WINDOWS_AI_MACHINELEARNING_VERSION` before configuring if you need to
test a different `Microsoft.Windows.AI.MachineLearning` package version.

## Build

From this directory:

```powershell
cmake -S . -B out\build -G "Visual Studio 18 2026" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static-md

cmake --build out\build --config Debug --target VerifyWinML
```

## Run

```powershell
.\out\build\Debug\VerifyWinML.exe
```

## What it tests

1. **EP Discovery** - Lists all available execution providers.
2. **EP Download & Registration** - Downloads and registers the available WinML EPs.
3. **Model Catalog** - Lists text model variants backed by registered accelerated EPs.
4. **Streaming Chat** - Runs streaming chat completion on a WinML EP-backed model via the native C++ SDK.
