# Model

Namespace: Microsoft.AI.Foundry.Local

```csharp
public class Model : IModel
```

Inheritance [Object](https://docs.microsoft.com/en-us/dotnet/api/system.object) → [Model](./microsoft.ai.foundry.local.model.md)<br>
Implements [IModel](./microsoft.ai.foundry.local.imodel.md)<br>
Attributes [NullableContextAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.nullablecontextattribute), [NullableAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.nullableattribute)

## Properties

### **Variants**

```csharp
public IReadOnlyList<IModel> Variants { get; }
```

#### Property Value

[IReadOnlyList&lt;IModel&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.collections.generic.ireadonlylist-1)<br>

### **Alias**

```csharp
public string Alias { get; set; }
```

#### Property Value

[String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

### **Id**

```csharp
public string Id { get; }
```

#### Property Value

[String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

### **Info**

```csharp
public ModelInfo Info { get; }
```

#### Property Value

[ModelInfo](./microsoft.ai.foundry.local.modelinfo.md)<br>

## Methods

### **IsCachedAsync(Nullable&lt;CancellationToken&gt;)**

Is the currently selected variant cached locally?

```csharp
public Task<bool> IsCachedAsync(Nullable<CancellationToken> ct)
```

#### Parameters

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>

#### Returns

[Task&lt;Boolean&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>

### **IsLoadedAsync(Nullable&lt;CancellationToken&gt;)**

Is the currently selected variant loaded in memory?

```csharp
public Task<bool> IsLoadedAsync(Nullable<CancellationToken> ct)
```

#### Parameters

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>

#### Returns

[Task&lt;Boolean&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>

### **SelectVariant(IModel)**

Select a specific model variant from [Model.Variants](./microsoft.ai.foundry.local.model.md#variants) to use for [IModel](./microsoft.ai.foundry.local.imodel.md) operations.

```csharp
public void SelectVariant(IModel variant)
```

#### Parameters

`variant` [IModel](./microsoft.ai.foundry.local.imodel.md)<br>
Model variant to select. Must be one of the variants in [Model.Variants](./microsoft.ai.foundry.local.model.md#variants).

#### Exceptions

[FoundryLocalException](./microsoft.ai.foundry.local.foundrylocalexception.md)<br>
If variant is not valid for this model.

### **GetPathAsync(Nullable&lt;CancellationToken&gt;)**

```csharp
public Task<string> GetPathAsync(Nullable<CancellationToken> ct)
```

#### Parameters

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>

#### Returns

[Task&lt;String&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>

### **DownloadAsync(Action&lt;Single&gt;, Nullable&lt;CancellationToken&gt;)**

```csharp
public Task DownloadAsync(Action<float> downloadProgress, Nullable<CancellationToken> ct)
```

#### Parameters

`downloadProgress` [Action&lt;Single&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.action-1)<br>

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>

#### Returns

[Task](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task)<br>

### **LoadAsync(Nullable&lt;CancellationToken&gt;)**

```csharp
public Task LoadAsync(Nullable<CancellationToken> ct)
```

#### Parameters

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>

#### Returns

[Task](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task)<br>

### **GetChatClientAsync(Nullable&lt;CancellationToken&gt;)**

```csharp
public Task<OpenAIChatClient> GetChatClientAsync(Nullable<CancellationToken> ct)
```

#### Parameters

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>

#### Returns

[Task&lt;OpenAIChatClient&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>

### **GetAudioClientAsync(Nullable&lt;CancellationToken&gt;)**

```csharp
public Task<OpenAIAudioClient> GetAudioClientAsync(Nullable<CancellationToken> ct)
```

#### Parameters

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>

#### Returns

[Task&lt;OpenAIAudioClient&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>

### **UnloadAsync(Nullable&lt;CancellationToken&gt;)**

```csharp
public Task UnloadAsync(Nullable<CancellationToken> ct)
```

#### Parameters

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>

#### Returns

[Task](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task)<br>

### **RemoveFromCacheAsync(Nullable&lt;CancellationToken&gt;)**

```csharp
public Task RemoveFromCacheAsync(Nullable<CancellationToken> ct)
```

#### Parameters

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>

#### Returns

[Task](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task)<br>
