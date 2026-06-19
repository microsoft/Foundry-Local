# 🚀 Foundry Local C# Samples

These samples demonstrate how to use the Foundry Local C# SDK.

They **track `main`** and consume the SDK **from local source** via a `ProjectReference` to
`sdk/cs/src/Microsoft.AI.Foundry.Local.csproj` — they are **not** pinned to a published package
version. The `Microsoft.AI.Foundry.Local.Core*` packages and native runtime assets flow
transitively from that SDK project and restore from nuget.org, along with any third-party packages.

## Samples

| Sample | Description |
|---|---|
| [chat-completion](chat-completion/) | Run the same chat prompt two ways: native in-process inference **and** the local OpenAI-compatible web server (`/v1/chat/completions`). |
| [embeddings](embeddings/) | Generate single and batch text embeddings using the Foundry Local SDK. |
| [audio](audio/) | Live microphone streaming transcription (Nemotron ASR) **and** file-based transcription (Whisper) via `--file [path]`. |
| [responses-api](responses-api/) | Stream a vision (image understanding) response from the local web server using the Responses API. |

## Running a sample

1. Clone the repository:
   ```bash
   git clone https://github.com/microsoft/Foundry-Local.git
   cd Foundry-Local/samples/cs
   ```

2. Build and run a sample (the SDK is resolved from `sdk/cs` source via the project reference;
   `Microsoft.AI.Foundry.Local.Core` and third-party packages restore from nuget.org):
   ```bash
   cd chat-completion
   dotnet build
   dotnet run
   ```
