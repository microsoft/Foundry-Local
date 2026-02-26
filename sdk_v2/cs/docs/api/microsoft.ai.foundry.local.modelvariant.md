# ModelVariant

Namespace: Microsoft.AI.Foundry.Local

```csharp
public class ModelVariant : IModel
```

Inheritance [Object](https://docs.microsoft.com/en-us/dotnet/api/system.object) → [ModelVariant](./microsoft.ai.foundry.local.modelvariant.md)<br>
Implements [IModel](./microsoft.ai.foundry.local.imodel.md)<br>
Attributes [NullableContextAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.nullablecontextattribute), [NullableAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.nullableattribute)

## Properties

### **Info**

```csharp
public ModelInfo Info { get; }
```

#### Property Value

[ModelInfo](./microsoft.ai.foundry.local.modelinfo.md)<br>

### **Id**

```csharp
public string Id { get; }
```

#### Property Value

[String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

### **Alias**

```csharp
public string Alias { get; }
```

#### Property Value

[String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

### **Version**

```csharp
public int Version { get; set; }
```

#### Property Value

[Int32](https://docs.microsoft.com/en-us/dotnet/api/system.int32)<br>

## Methods

### **IsLoadedAsync(Nullable&lt;CancellationToken&gt;)**

```csharp
public Task<bool> IsLoadedAsync(Nullable<CancellationToken> ct)
```

#### Parameters

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>

#### Returns

[Task&lt;Boolean&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>

### **IsCachedAsync(Nullable&lt;CancellationToken&gt;)**

```csharp
public Task<bool> IsCachedAsync(Nullable<CancellationToken> ct)
```

#### Parameters

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>

#### Returns

[Task&lt;Boolean&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>

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

