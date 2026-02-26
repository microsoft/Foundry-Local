# OpenAIChatClient

Namespace: Microsoft.AI.Foundry.Local

Chat Client that uses the OpenAI API.
 Implemented using Betalgo.Ranul.OpenAI SDK types.

```csharp
public class OpenAIChatClient
```

Inheritance [Object](https://docs.microsoft.com/en-us/dotnet/api/system.object) → [OpenAIChatClient](./microsoft.ai.foundry.local.openaichatclient.md)<br>
Attributes [NullableContextAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.nullablecontextattribute), [NullableAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.nullableattribute)

## Properties

### **Settings**

Settings to use for chat completions using this client.

```csharp
public ChatSettings Settings { get; }
```

#### Property Value

[ChatSettings](./microsoft.ai.foundry.local.openaichatclient.chatsettings.md)<br>

## Methods

### **CompleteChatAsync(IEnumerable&lt;ChatMessage&gt;, Nullable&lt;CancellationToken&gt;)**

Execute a chat completion request.
 
 To continue a conversation, add the ChatMessage from the previous response and new prompt to the messages.

```csharp
public Task<ChatCompletionCreateResponse> CompleteChatAsync(IEnumerable<ChatMessage> messages, Nullable<CancellationToken> ct)
```

#### Parameters

`messages` [IEnumerable&lt;ChatMessage&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.collections.generic.ienumerable-1)<br>
Chat messages. The system message is automatically added.

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>
Optional cancellation token.

#### Returns

[Task&lt;ChatCompletionCreateResponse&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>
Chat completion response.

### **CompleteChatStreamingAsync(IEnumerable&lt;ChatMessage&gt;, CancellationToken)**

Execute a chat completion request with streamed output.
 
 To continue a conversation, add the ChatMessage from the previous response and new prompt to the messages.

```csharp
public IAsyncEnumerable<ChatCompletionCreateResponse> CompleteChatStreamingAsync(IEnumerable<ChatMessage> messages, CancellationToken ct)
```

#### Parameters

`messages` [IEnumerable&lt;ChatMessage&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.collections.generic.ienumerable-1)<br>
Chat messages. The system message is automatically added.

`ct` [CancellationToken](https://docs.microsoft.com/en-us/dotnet/api/system.threading.cancellationtoken)<br>
Optional cancellation token.

#### Returns

[IAsyncEnumerable&lt;ChatCompletionCreateResponse&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.collections.generic.iasyncenumerable-1)<br>
Async enumerable of chat completion responses.

