# Foundry Local C# SDK (under active development)

> [!NOTE]
> This SDK is under active development and may not be fully functional.

## Installation

To install the SDK, run the following command in your terminal:

```bash
cd sdk/cs
dotnet build
```

## Usage

```csharp
using Microsoft.AI.Foundry.Local;
using OpenAI;
using OpenAI.Chat;
using System.ClientModel;
using System.Diagnostics.Metrics;

var alias = "deepseek-r1-1.5b";

var manager = await FoundryManager.StartModelAsync(aliasOrModelId: alias);

var model = await manager.GetModelInfoAsync(aliasOrModelId: alias);
ApiKeyCredential key = new ApiKeyCredential(manager.ApiKey);
OpenAIClient client = new OpenAIClient(key, new OpenAIClientOptions
{
    Endpoint = manager.Endpoint
});

var chatClient = client.GetChatClient(model?.ModelId);

CollectionResult<StreamingChatCompletionUpdate> completionUpdates = chatClient.CompleteChatStreaming("Why is the sky blue'");

Console.Write($"[ASSISTANT]: ");
foreach (StreamingChatCompletionUpdate completionUpdate in completionUpdates)
{
    if (completionUpdate.ContentUpdate.Count > 0)
    {
        Console.Write(completionUpdate.ContentUpdate[0].Text);
    }
}
```
