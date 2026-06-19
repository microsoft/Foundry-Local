# Vision via Web Server (Responses API) Example

Stream a vision (image understanding) response from the local Foundry web server using the
OpenAI-compatible **Responses API** (`/v1/responses`).

## Prerequisites

- [Foundry Local](https://github.com/microsoft/Foundry-Local) installed
- .NET 9 SDK
- A vision-capable model (e.g. `qwen2.5-vl-3b`)

## SDK consumption

This sample tracks `main`: it consumes the Foundry Local C# SDK **from local source** via a
`ProjectReference` to `sdk/cs/src/Microsoft.AI.Foundry.Local.csproj`. It is **not** version-pinned
to a published package. The `Microsoft.AI.Foundry.Local.Core*` packages and native runtime assets
flow transitively from that project and restore from nuget.org.

## Build & run

```bash
# from this directory
dotnet build

# describe the bundled test image with a vision model
dotnet run -- <model_alias_or_id>

# describe your own image
dotnet run -- <model_alias_or_id> /path/to/image.jpg

# list vision models in the catalog
dotnet run -- --list-models
```

`dotnet build` resolves the SDK from `sdk/cs` source via the project reference and restores
`Microsoft.AI.Foundry.Local.Core` from nuget.org.

## What it does

1. Initializes the SDK and downloads/registers execution providers.
2. Downloads and loads the requested vision model.
3. Starts the local web server.
4. Base64-encodes the image and POSTs a streaming request to `/v1/responses` with
   `input_text` + `input_image` content parts.
5. Streams `response.output_text.delta` events to the console.
6. Stops the web server and unloads the model.
