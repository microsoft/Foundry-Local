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
Task<List<IModel>> ListModelsAsync(Nullable<CancellationToken> ct)
```

#### Parameters

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>
Optional CancellationToken.

#### Returns

[Task&lt;List&lt;IModel&gt;&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>
List of IModel instances.

### **GetModelAsync(String, Nullable&lt;CancellationToken&gt;)**

Lookup a model by its alias.

```csharp
Task<IModel> GetModelAsync(string modelAlias, Nullable<CancellationToken> ct)
```

#### Parameters

`modelAlias` [String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>
Model alias.

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>
Optional CancellationToken.

#### Returns

[Task&lt;IModel&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>
The matching IModel, or null if no model with the given alias exists.

### **GetModelVariantAsync(String, Nullable&lt;CancellationToken&gt;)**

Lookup a model variant by its unique model id.
 NOTE: This will return an IModel with a single variant. Use GetModelAsync to get an IModel with all available
 variants.

```csharp
Task<IModel> GetModelVariantAsync(string modelId, Nullable<CancellationToken> ct)
```

#### Parameters

`modelId` [String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>
Model id.

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>
Optional CancellationToken.

#### Returns

[Task&lt;IModel&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>
The matching IModel, or null if no variant with the given id exists.

### **GetCachedModelsAsync(Nullable&lt;CancellationToken&gt;)**

Get a list of currently downloaded models from the model cache.

```csharp
Task<List<IModel>> GetCachedModelsAsync(Nullable<CancellationToken> ct)
```

#### Parameters

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>
Optional CancellationToken.

#### Returns

[Task&lt;List&lt;IModel&gt;&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>
List of IModel instances.

### **GetLoadedModelsAsync(Nullable&lt;CancellationToken&gt;)**

Get a list of the currently loaded models.

```csharp
Task<List<IModel>> GetLoadedModelsAsync(Nullable<CancellationToken> ct)
```

#### Parameters

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>
Optional CancellationToken.

#### Returns

[Task&lt;List&lt;IModel&gt;&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>
List of IModel instances.

### **GetLatestVersionAsync(IModel, Nullable&lt;CancellationToken&gt;)**

Get the latest version of a model.
 This is used to check if a newer version of a model is available in the catalog for download.

```csharp
Task<IModel> GetLatestVersionAsync(IModel model, Nullable<CancellationToken> ct)
```

#### Parameters

`model` [IModel](./microsoft.ai.foundry.local.imodel.md)<br>
The model to check for the latest version.

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>
Optional cancellation token.

#### Returns

[Task&lt;IModel&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>
The latest version of the model. Will match the input if it is the latest version.
