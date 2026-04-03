# OpenAIEmbeddingClient

Namespace: Microsoft.AI.Foundry.Local

Embedding Client that uses the OpenAI API.
 Implemented using Betalgo.Ranul.OpenAI SDK types.

```csharp
public class OpenAIEmbeddingClient
```

Inheritance [Object](https://docs.microsoft.com/en-us/dotnet/api/system.object) → [OpenAIEmbeddingClient](./microsoft.ai.foundry.local.openaiembeddingclient.md)<br>
Attributes [NullableContextAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.nullablecontextattribute), [NullableAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.nullableattribute)

## Properties

### **Settings**

Settings to use for embedding requests using this client.

```csharp
public EmbeddingSettings Settings { get; }
```

#### Property Value

EmbeddingSettings<br>

## Methods

### **GenerateEmbeddingAsync(String, Nullable&lt;CancellationToken&gt;)**

Generate embeddings for the given input text.

```csharp
public Task<EmbeddingCreateResponse> GenerateEmbeddingAsync(string input, Nullable<CancellationToken> ct)
```

#### Parameters

`input` [String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>
The text to generate embeddings for.

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>
Optional cancellation token.

#### Returns

[Task&lt;EmbeddingCreateResponse&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>
Embedding response containing the embedding vector.
