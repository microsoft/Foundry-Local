# Embeddings Example

Generate single and batch text embeddings natively (in-process) with the Foundry Local C# SDK.

## Prerequisites

- [Foundry Local](https://github.com/microsoft/Foundry-Local) installed
- .NET 9 SDK

## SDK consumption

This sample tracks `main`: it consumes the Foundry Local C# SDK **from local source** via a
`ProjectReference` to `sdk/cs/src/Microsoft.AI.Foundry.Local.csproj`. It is **not** version-pinned
to a published package. The `Microsoft.AI.Foundry.Local.Core*` packages and native runtime assets
flow transitively from that project and restore from nuget.org.

## Build & run

```bash
# from this directory
dotnet build
dotnet run
```

`dotnet build` resolves the SDK from `sdk/cs` source via the project reference and restores
`Microsoft.AI.Foundry.Local.Core` from nuget.org.

## What it does

1. Initializes the SDK and downloads/loads the `qwen3-embedding-0.6b` model.
2. Generates a single embedding and prints its dimensions and first values.
3. Generates a batch of embeddings and prints the dimensions for each.
4. Unloads the model.
