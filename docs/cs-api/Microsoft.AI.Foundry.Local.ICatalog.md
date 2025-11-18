# <a id="Microsoft_AI_Foundry_Local_ICatalog"></a> Interface ICatalog

Namespace: [Microsoft.AI.Foundry.Local](Microsoft.AI.Foundry.Local.md)  
Assembly: Microsoft.AI.Foundry.Local.dll  

```csharp
public interface ICatalog
```

## Properties

### <a id="Microsoft_AI_Foundry_Local_ICatalog_Name"></a> Name

The catalog name.

```csharp
string Name { get; }
```

#### Property Value

 [string](https://learn.microsoft.com/dotnet/api/system.string)

## Methods

### <a id="Microsoft_AI_Foundry_Local_ICatalog_GetCachedModelsAsync_System_Nullable_System_Threading_CancellationToken__"></a> GetCachedModelsAsync\(CancellationToken?\)

Get the list of currently downloaded models available in the local cache.

```csharp
Task<List<ModelVariant>> GetCachedModelsAsync(CancellationToken? ct = null)
```

#### Parameters

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)?

Optional CancellationToken.

#### Returns

 [Task](https://learn.microsoft.com/dotnet/api/system.threading.tasks.task\-1)<[List](https://learn.microsoft.com/dotnet/api/system.collections.generic.list\-1)<[ModelVariant](Microsoft.AI.Foundry.Local.ModelVariant.md)\>\>

List of ModelVariant instances.

### <a id="Microsoft_AI_Foundry_Local_ICatalog_GetLoadedModelsAsync_System_Nullable_System_Threading_CancellationToken__"></a> GetLoadedModelsAsync\(CancellationToken?\)

Get a list of the currently loaded models.

```csharp
Task<List<ModelVariant>> GetLoadedModelsAsync(CancellationToken? ct = null)
```

#### Parameters

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)?

Optional CancellationToken.

#### Returns

 [Task](https://learn.microsoft.com/dotnet/api/system.threading.tasks.task\-1)<[List](https://learn.microsoft.com/dotnet/api/system.collections.generic.list\-1)<[ModelVariant](Microsoft.AI.Foundry.Local.ModelVariant.md)\>\>

List of ModelVariant instances.

### <a id="Microsoft_AI_Foundry_Local_ICatalog_GetModelAsync_System_String_System_Nullable_System_Threading_CancellationToken__"></a> GetModelAsync\(string, CancellationToken?\)

Lookup a model by its alias.

```csharp
Task<Model?> GetModelAsync(string modelAlias, CancellationToken? ct = null)
```

#### Parameters

`modelAlias` [string](https://learn.microsoft.com/dotnet/api/system.string)

Model alias.

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)?

Optional CancellationToken.

#### Returns

 [Task](https://learn.microsoft.com/dotnet/api/system.threading.tasks.task\-1)<[Model](Microsoft.AI.Foundry.Local.Model.md)?\>

Model if found.

### <a id="Microsoft_AI_Foundry_Local_ICatalog_GetModelVariantAsync_System_String_System_Nullable_System_Threading_CancellationToken__"></a> GetModelVariantAsync\(string, CancellationToken?\)

Lookup a model variant by its unique model id.

```csharp
Task<ModelVariant?> GetModelVariantAsync(string modelId, CancellationToken? ct = null)
```

#### Parameters

`modelId` [string](https://learn.microsoft.com/dotnet/api/system.string)

Model id.

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)?

Optional CancellationToken.

#### Returns

 [Task](https://learn.microsoft.com/dotnet/api/system.threading.tasks.task\-1)<[ModelVariant](Microsoft.AI.Foundry.Local.ModelVariant.md)?\>

Model variant if found.

### <a id="Microsoft_AI_Foundry_Local_ICatalog_ListModelsAsync_System_Nullable_System_Threading_CancellationToken__"></a> ListModelsAsync\(CancellationToken?\)

List the available models in the catalog.

```csharp
Task<List<Model>> ListModelsAsync(CancellationToken? ct = null)
```

#### Parameters

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)?

Optional CancellationToken.

#### Returns

 [Task](https://learn.microsoft.com/dotnet/api/system.threading.tasks.task\-1)<[List](https://learn.microsoft.com/dotnet/api/system.collections.generic.list\-1)<[Model](Microsoft.AI.Foundry.Local.Model.md)\>\>

List of Model instances.

