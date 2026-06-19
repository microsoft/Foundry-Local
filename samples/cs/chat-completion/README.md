# Native Chat Completions Example

Run the same chat prompt against Foundry Local two ways from a single program:

1. **Native, in-process inference** via the SDK's chat client (no web server involved).
2. **The local OpenAI-compatible web server** (`/v1/chat/completions`) via the [OpenAI .NET SDK](https://www.nuget.org/packages/OpenAI).

The program prints clear section headers so you can compare the two paths:

```
=== Native inference ===
[ASSISTANT]: ...

=== Web server (/v1/chat/completions) ===
[ASSISTANT]: ...
```

## Prerequisites

- [Foundry Local](https://github.com/microsoft/Foundry-Local) installed
- .NET 9 SDK

## SDK consumption

This sample tracks `main`: it consumes the Foundry Local C# SDK **from local source** via a
`ProjectReference` to `sdk/cs/src/Microsoft.AI.Foundry.Local.csproj`. It is **not** version-pinned
to a published package. The `Microsoft.AI.Foundry.Local.Core*` packages and native runtime assets
flow transitively from that project and restore from nuget.org, along with the third-party
`OpenAI` package.

## Build & run

```bash
# from this directory
dotnet build
dotnet run
```

`dotnet build` resolves the SDK from `sdk/cs` source via the project reference and restores
`Microsoft.AI.Foundry.Local.Core` plus third-party packages from nuget.org.

## What it does

1. Initializes the SDK and downloads/registers execution providers.
2. Downloads and loads the `qwen2.5-0.5b` model.
3. Streams the prompt through the native chat client.
4. Starts the local web server and streams the same prompt through the OpenAI SDK.
5. Stops the web server and unloads the model.
