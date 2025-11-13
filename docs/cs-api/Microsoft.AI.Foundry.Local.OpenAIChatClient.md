# <a id="Microsoft_AI_Foundry_Local_OpenAIChatClient"></a> Class OpenAIChatClient

Namespace: [Microsoft.AI.Foundry.Local](Microsoft.AI.Foundry.Local.md)  
Assembly: Microsoft.AI.Foundry.Local.dll  

Chat client using an OpenAI compatible API surface implemented using Betalgo.Ranul.OpenAI SDK types.
Provides convenience methods for standard and streaming chat completions.

```csharp
public class OpenAIChatClient
```

#### Inheritance

[object](https://learn.microsoft.com/dotnet/api/system.object) ← 
[OpenAIChatClient](Microsoft.AI.Foundry.Local.OpenAIChatClient.md)

#### Inherited Members

[object.Equals\(object?\)](https://learn.microsoft.com/dotnet/api/system.object.equals\#system\-object\-equals\(system\-object\)), 
[object.Equals\(object?, object?\)](https://learn.microsoft.com/dotnet/api/system.object.equals\#system\-object\-equals\(system\-object\-system\-object\)), 
[object.GetHashCode\(\)](https://learn.microsoft.com/dotnet/api/system.object.gethashcode), 
[object.GetType\(\)](https://learn.microsoft.com/dotnet/api/system.object.gettype), 
[object.MemberwiseClone\(\)](https://learn.microsoft.com/dotnet/api/system.object.memberwiseclone), 
[object.ReferenceEquals\(object?, object?\)](https://learn.microsoft.com/dotnet/api/system.object.referenceequals), 
[object.ToString\(\)](https://learn.microsoft.com/dotnet/api/system.object.tostring)

## Properties

### <a id="Microsoft_AI_Foundry_Local_OpenAIChatClient_Settings"></a> Settings

Settings to use for chat completions using this client.

```csharp
public OpenAIChatClient.ChatSettings Settings { get; }
```

#### Property Value

 [OpenAIChatClient](Microsoft.AI.Foundry.Local.OpenAIChatClient.md).[ChatSettings](Microsoft.AI.Foundry.Local.OpenAIChatClient.ChatSettings.md)

## Methods

### <a id="Microsoft_AI_Foundry_Local_OpenAIChatClient_CompleteChatAsync_System_Collections_Generic_IEnumerable_Betalgo_Ranul_OpenAI_ObjectModels_RequestModels_ChatMessage__System_Nullable_System_Threading_CancellationToken__"></a> CompleteChatAsync\(IEnumerable<ChatMessage\>, CancellationToken?\)

Execute a chat completion request.
To continue a conversation, add prior response messages and new prompt to the messages list.

```csharp
public Task<ChatCompletionCreateResponse> CompleteChatAsync(IEnumerable<ChatMessage> messages, CancellationToken? ct = null)
```

#### Parameters

`messages` [IEnumerable](https://learn.microsoft.com/dotnet/api/system.collections.generic.ienumerable\-1)<ChatMessage\>

Chat messages including system / user / assistant roles.

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)?

Optional cancellation token.

#### Returns

 [Task](https://learn.microsoft.com/dotnet/api/system.threading.tasks.task\-1)<ChatCompletionCreateResponse\>

Chat completion response.

### <a id="Microsoft_AI_Foundry_Local_OpenAIChatClient_CompleteChatStreamingAsync_System_Collections_Generic_IEnumerable_Betalgo_Ranul_OpenAI_ObjectModels_RequestModels_ChatMessage__System_Threading_CancellationToken_"></a> CompleteChatStreamingAsync\(IEnumerable<ChatMessage\>, CancellationToken\)

Execute a chat completion request with streamed output.
To continue a conversation, add prior response messages and new prompt to the messages list.

```csharp
public IAsyncEnumerable<ChatCompletionCreateResponse> CompleteChatStreamingAsync(IEnumerable<ChatMessage> messages, CancellationToken ct)
```

#### Parameters

`messages` [IEnumerable](https://learn.microsoft.com/dotnet/api/system.collections.generic.ienumerable\-1)<ChatMessage\>

Chat messages including system / user / assistant roles.

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)

Cancellation token.

#### Returns

 [IAsyncEnumerable](https://learn.microsoft.com/dotnet/api/system.collections.generic.iasyncenumerable\-1)<ChatCompletionCreateResponse\>

Async enumerable producing incremental chat completion responses.

