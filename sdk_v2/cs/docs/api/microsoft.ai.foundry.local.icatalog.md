# ICatalog

Namespace: Microsoft.AI.Foundry.Local

```csharp
public interface ICatalog
```

Attributes [NullableContextAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.nullablecontextattribute)

## Properties

### **Name**

The catalog name.

```csharp
public abstract string Name { get; }
```

#### Property Value

[String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

## Methods

### **ListModelsAsync(Nullable&lt;CancellationToken&gt;)**

List the available models in the catalog.

```csharp
Task<List<Model>> ListModelsAsync(Nullable<CancellationToken> ct)
```

#### Parameters

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>
Optional CancellationToken.

#### Returns

[Task&lt;List&lt;Model&gt;&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>
List of Model instances.

### **GetModelAsync(String, Nullable&lt;CancellationToken&gt;)**

Lookup a model by its alias.

```csharp
Task<Model> GetModelAsync(string modelAlias, Nullable<CancellationToken> ct)
```

#### Parameters

`modelAlias` [String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>
Model alias.

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>
Optional CancellationToken.

#### Returns

[Task&lt;Model&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>
The matching Model, or null if no model with the given alias exists.

### **GetModelVariantAsync(String, Nullable&lt;CancellationToken&gt;)**

Lookup a model variant by its unique model id.

```csharp
Task<ModelVariant> GetModelVariantAsync(string modelId, Nullable<CancellationToken> ct)
```

#### Parameters

`modelId` [String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>
Model id.

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>
Optional CancellationToken.

#### Returns

[Task&lt;ModelVariant&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>
The matching ModelVariant, or null if no variant with the given id exists.

### **GetCachedModelsAsync(Nullable&lt;CancellationToken&gt;)**

Get a list of currently downloaded models from the model cache.

```csharp
Task<List<ModelVariant>> GetCachedModelsAsync(Nullable<CancellationToken> ct)
```

#### Parameters

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>
Optional CancellationToken.

#### Returns

[Task&lt;List&lt;ModelVariant&gt;&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>
List of ModelVariant instances.

### **GetLoadedModelsAsync(Nullable&lt;CancellationToken&gt;)**

Get a list of the currently loaded models.

```csharp
Task<List<ModelVariant>> GetLoadedModelsAsync(Nullable<CancellationToken> ct)
```

#### Parameters

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>
Optional CancellationToken.

#### Returns

[Task&lt;List&lt;ModelVariant&gt;&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>
List of ModelVariant instances.

