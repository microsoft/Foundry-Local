# Microsoft.AI.Foundry.Local

Control the Microsoft AI Foundry Local service from your C# application.

## Overview

Use Microsoft.AI.Foundry.Local to connect your app to an installed Microsoft.Foundry service.

## Getting Started

Install the Foundry Local client and service from http://github.com/microsoft/Foundry-Local.

Install the NuGet packages:

```powershell
dotnet add package Microsoft.AI.Foundry.Local
dotnet add package OpenAI
```

Add using directives to your code:
```csharp
using Microsoft.AI.Foundry.Local;
using OpenAI;
```

Create a new instance of the `FoundryManager` class and start your model:
```csharp
var modelAlias = "qwen2.5-0.5b";
var manager = await FoundryManager.StartModelAsync(modelAlias);
var model = await manager.GetModelInfoAsync(modelAlias);
```

Connect the OpenAI Client to the model running in Foundry Local:
```csharp
ApiKeyCredential key = new ApiKeyCredential(manager.ApiKey);
OpenAIClient client = new OpenAIClient(key, new OpenAIClientOptions
{
    Endpoint = manager.Endpoint
});
var chatClient = client.GetChatClient(model?.ModelId);
```
