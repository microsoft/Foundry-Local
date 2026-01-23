# Foundry Local C# SDK

## Installation

To use the Foundry Local C# SDK, you need to install the NuGet package:

```bash
dotnet add package Microsoft.AI.Foundry.Local
```

### Building from source
To build the SDK, run the following command in your terminal:

```bash
cd sdk/cs
dotnet build sdk_v2/cs/src/Microsoft.AI.Foundry.Local.csproj
```

You can also load [FoundryLocal.sln](./FoundryLocal.sln) in Visual Studio 2022 or VSCode.

## Usage

> [!NOTE]
> For this example, you'll need the OpenAI Nuget package installed as well:
> ```bash
> dotnet add package OpenAI
> ```

```csharp
using Microsoft.AI.Foundry.Local;
using OpenAI;
using OpenAI.Chat;
using System.ClientModel;
using System.Diagnostics.Metrics;

var alias = "phi-3.5-mini";

var manager = await FoundryLocalManager.StartModelAsync(aliasOrModelId: alias);

var model = await manager.GetModelInfoAsync(aliasOrModelId: alias);
ApiKeyCredential key = new ApiKeyCredential(manager.ApiKey);
OpenAIClient client = new OpenAIClient(key, new OpenAIClientOptions
{
    Endpoint = manager.Endpoint
});

var chatClient = client.GetChatClient(model?.ModelId);

var completionUpdates = chatClient.CompleteChatStreaming("Why is the sky blue'");

Console.Write($"[ASSISTANT]: ");
foreach (var completionUpdate in completionUpdates)
{
    if (completionUpdate.ContentUpdate.Count > 0)
    {
        Console.Write(completionUpdate.ContentUpdate[0].Text);
    }
}
```
