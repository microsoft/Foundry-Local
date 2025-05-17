# Foundry Local C# SDK (under active development)

> This SDK is under active development and may not be fully functional.

## Installation

To build the SDK, run the following command in your terminal:

```bash
cd sdk/cs
dotnet build
```

You can also load [FoundryLocal.sln](./FoundryLocal.sln) in Visual Studio 2022 or VSCode. Update your
`nuget.config` to include the local path to the generated NuGet package:

```xml
<configuration>
  <packageSources>
    <add key="foundry-local" value="C:\path\to\foundry-local\sdk\cs\bin\Debug" />
  </packageSources>
</configuration>
```

Then, install the package using the following command:

```bash
dotnet add package FoundryLocal --source foundry-local
```

An official NuGet package will be available soon.

## Usage

```csharp
using Microsoft.AI.Foundry.Local;
using OpenAI;
using OpenAI.Chat;
using System.ClientModel;
using System.Diagnostics.Metrics;

var alias = "phi-3.5-mini";

var manager = await FoundryManager.StartModelAsync(aliasOrModelId: alias);

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
