# Foundry Local Responses web service sample (C#)

This sample starts the Foundry Local OpenAI-compatible web service, then uses the official OpenAI .NET SDK to call the Responses API.

The pattern is:

1. `FoundryLocalManager` handles Foundry Local setup, model download/load, web service startup, and cleanup.
1. `OpenAI.Responses.ResponsesClient` (from the official `OpenAI` NuGet package) handles the actual `/v1/responses` calls.

## Prerequisites

- .NET 9 SDK
- Internet access on first run to download the sample model

## What the sample does

1. Initializes `FoundryLocalManager`.
1. Downloads and registers execution providers.
1. Downloads and loads `qwen2.5-0.5b`.
1. Starts the local web service at `http://127.0.0.1:52495`.
1. Creates an `OpenAIClient` pointed at `http://127.0.0.1:52495/v1`.
1. Runs a non-streaming Responses call.
1. Runs a streaming Responses call (`StreamingResponseOutputTextDeltaUpdate` events).
1. Runs a Responses function-calling flow with a sample `get_weather` tool, then submits a tool result back via `previous_response_id`.
1. Stops the web service and unloads the model.

## Run the sample

```powershell
cd samples/cs/responses-foundry-local-web-server
dotnet run
```

## Expected output

```text
=== Non-streaming ===
[ASSISTANT]: 4

=== Streaming ===
[ASSISTANT]: 1, 2, 3.

=== Function calling ===
Tool call: get_weather()
Tool output: {"location": "Seattle", "weather": "72 degrees F and sunny"}
[ASSISTANT]: It's 72 degrees F and sunny in Seattle.
```

The exact model text varies.

## Troubleshooting

If the sample fails while creating `FoundryLocalManager` with a native symbol error such as `Failed to resolve 'execute_command_with_binary' symbol`, the installed Foundry Local Core runtime is older than the native bits expect. Try the latest stable `Microsoft.AI.Foundry.Local[.WinML]` package, or a recent ORT-Nightly package if needed.

If port `52495` is already in use, edit `Program.cs` and change `config.Web.Urls`.
