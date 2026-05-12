# OpenAIEmbeddingClient

Namespace: Microsoft.AI.Foundry.Local

Embedding Client that uses the OpenAI API.
 Implemented using Betalgo.Ranul.OpenAI SDK types.

```csharp
public class OpenAIEmbeddingClient
```

Inheritance [Object](https://docs.microsoft.com/en-us/dotnet/api/system.object) → [OpenAIEmbeddingClient](./microsoft.ai.foundry.local.openaiembeddingclient.md)<br>
Attributes [NullableContextAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.nullablecontextattribute), [NullableAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.nullableattribute)

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

### **GenerateEmbeddingsAsync(IEnumerable&lt;String&gt;, Nullable&lt;CancellationToken&gt;)**

Generate embeddings for multiple input texts in a single request.

```csharp
public Task<EmbeddingCreateResponse> GenerateEmbeddingsAsync(IEnumerable<string> inputs, Nullable<CancellationToken> ct)
```

#### Parameters

`inputs` [IEnumerable&lt;String&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.collections.generic.ienumerable-1)<br>
The texts to generate embeddings for.

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>
Optional cancellation token.

#### Returns

[Task&lt;EmbeddingCreateResponse&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>
Embedding response containing one embedding vector per input.
