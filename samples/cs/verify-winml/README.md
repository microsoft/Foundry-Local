# Verify WinML 2.0 Execution Providers (C#)

This sample verifies that WinML 2.0 execution providers are correctly discovered,
downloaded, and registered using the Foundry Local C# SDK. It uses registered WinML
EP-backed model variants and finishes with one native streaming chat check.

## Prerequisites

- Windows with a compatible GPU
- Windows App SDK 2.0 runtime installed (preview1 or experimental)
- .NET 9.0 SDK

## Build & Run

This sample uses the public `Microsoft.AI.Foundry.Local.WinML` SDK package and
overrides its native `Microsoft.AI.Foundry.Local.Core.WinML` dependency with the
preview package from ORT-Nightly via the shared `..\nuget.config`.

```bash
dotnet run
```
