# IModel

Namespace: Microsoft.AI.Foundry.Local

```csharp
public interface IModel
```

Attributes [NullableContextAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.nullablecontextattribute)

## Properties

### **Id**

```csharp
public abstract string Id { get; }
```

#### Property Value

[String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

### **Alias**

```csharp
public abstract string Alias { get; }
```

#### Property Value

[String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

## Methods

### **IsCachedAsync(Nullable&lt;CancellationToken&gt;)**

```csharp
Task<bool> IsCachedAsync(Nullable<CancellationToken> ct)
```

#### Parameters

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>

#### Returns

[Task&lt;Boolean&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>

### **IsLoadedAsync(Nullable&lt;CancellationToken&gt;)**

```csharp
Task<bool> IsLoadedAsync(Nullable<CancellationToken> ct)
```

#### Parameters

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>

#### Returns

[Task&lt;Boolean&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>

### **DownloadAsync(Action&lt;Single&gt;, Nullable&lt;CancellationToken&gt;)**

Download the model to local cache if not already present.

```csharp
Task DownloadAsync(Action<float> downloadProgress, Nullable<CancellationToken> ct)
```

#### Parameters

`downloadProgress` [Action&lt;Single&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.action-1)<br>
Optional progress callback for download progress.
 Percentage download (0 - 100.0) is reported.

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>
Optional cancellation token.

#### Returns

[Task](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task)<br>

### **GetPathAsync(Nullable&lt;CancellationToken&gt;)**

Gets the model path if cached.

```csharp
Task<string> GetPathAsync(Nullable<CancellationToken> ct)
```

#### Parameters

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>
Optional cancellation token.

#### Returns

[Task&lt;String&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>
Path of model directory.

### **LoadAsync(Nullable&lt;CancellationToken&gt;)**

Load the model into memory if not already loaded.

```csharp
Task LoadAsync(Nullable<CancellationToken> ct)
```

#### Parameters

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>
Optional cancellation token.

#### Returns

[Task](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task)<br>

### **RemoveFromCacheAsync(Nullable&lt;CancellationToken&gt;)**

Remove the model from the local cache.

```csharp
Task RemoveFromCacheAsync(Nullable<CancellationToken> ct)
```

#### Parameters

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>
Optional cancellation token.

#### Returns

[Task](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task)<br>

### **UnloadAsync(Nullable&lt;CancellationToken&gt;)**

Unload the model if loaded.

```csharp
Task UnloadAsync(Nullable<CancellationToken> ct)
```

#### Parameters

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>
Optional cancellation token.

#### Returns

[Task](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task)<br>

### **GetChatClientAsync(Nullable&lt;CancellationToken&gt;)**

Get an OpenAI API based ChatClient

```csharp
Task<OpenAIChatClient> GetChatClientAsync(Nullable<CancellationToken> ct)
```

#### Parameters

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>
Optional cancellation token.

#### Returns

[Task&lt;OpenAIChatClient&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>
OpenAI.ChatClient

### **GetAudioClientAsync(Nullable&lt;CancellationToken&gt;)**

Get an OpenAI API based AudioClient

```csharp
Task<OpenAIAudioClient> GetAudioClientAsync(Nullable<CancellationToken> ct)
```

#### Parameters

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>
Optional cancellation token.

#### Returns

[Task&lt;OpenAIAudioClient&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>
OpenAI.AudioClient

