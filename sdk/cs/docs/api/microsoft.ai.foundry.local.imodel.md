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

### **Info**

```csharp
public abstract ModelInfo Info { get; }
```

#### Property Value

[ModelInfo](./microsoft.ai.foundry.local.modelinfo.md)<br>

### **Variants**

Variants of the model that are available. Variants of the model are optimized for different devices.

```csharp
public abstract IReadOnlyList<IModel> Variants { get; }
```

#### Property Value

[IReadOnlyList&lt;IModel&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.collections.generic.ireadonlylist-1)<br>

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

### **SelectVariant(IModel)**

Select a model variant from [IModel.Variants](./microsoft.ai.foundry.local.imodel.md#variants) to use for [IModel](./microsoft.ai.foundry.local.imodel.md) operations.
 An IModel from `Variants` can also be used directly.

```csharp
void SelectVariant(IModel variant)
```

#### Parameters

`variant` [IModel](./microsoft.ai.foundry.local.imodel.md)<br>
Model variant to select. Must be one of the variants in [IModel.Variants](./microsoft.ai.foundry.local.imodel.md#variants).

#### Exceptions

[FoundryLocalException](./microsoft.ai.foundry.local.foundrylocalexception.md)<br>
If variant is not valid for this model.
