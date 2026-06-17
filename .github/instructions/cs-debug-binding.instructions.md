---
description: Use when changing the C ABI (foundry_local_c.h) and validating C# tests, or when C# tests fail with stale-ABI symptoms (struct/layout mismatches, dispatch falling through unexpectedly).
applyTo: sdk_v2/cs/**
---

# C# Tests Auto-Load the C++ **Debug** Build

The C# project (`sdk_v2/cs/src/Microsoft.AI.Foundry.Local.csproj`) auto-detects
`sdk_v2/cpp/build/Windows/Debug/bin/Debug/foundry_local.dll` for inner-loop dev
and writes a `foundry_local.native.cfg` redirect file. The test project copies
this redirect to its output, so `dotnet test` always loads the **Debug** native
binary, never RelWithDebInfo.

## Implication

When you change the C ABI surface (e.g. `foundry_local_c.h`, struct layouts,
function signatures in `c_api.cc`), you **must rebuild Debug** before running
C# tests, even if you've already built RelWithDebInfo for the C++ test suite:

```powershell
cd sdk_v2/cpp
python build.py --build --config Debug
```

## Symptoms of a stale Debug DLL

- C# tests fail with native errors that don't match current C++ source line numbers
- Dispatch falls through unexpectedly (e.g. JSON pass-through stops working
  because the old DLL still expects the old item type tag)
- Marshalling appears correct in C# but the native side reads garbage
