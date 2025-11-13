# <a id="Microsoft_AI_Foundry_Local_ModelVariant"></a> Class ModelVariant

Namespace: [Microsoft.AI.Foundry.Local](Microsoft.AI.Foundry.Local.md)  
Assembly: Microsoft.AI.Foundry.Local.dll  

Represents a single, concrete downloadable model instance (a specific version + configuration) identified
by a unique model Id and grouped under a broader alias shared with other device-specific variants.
Provides:
 - Direct access to catalog metadata via <xref href="Microsoft.AI.Foundry.Local.ModelInfo" data-throw-if-not-resolved="false"></xref>
 - Lifecycle operations (download, load, unload, cache removal)
 - State queries (cached vs. loaded) independent of other variants
 - Resolution of the local cache path
 - Creation of OpenAI‑style chat and audio clients once loaded
Unlike <xref href="Microsoft.AI.Foundry.Local.Model" data-throw-if-not-resolved="false"></xref>, which orchestrates multiple variants under an alias, <xref href="Microsoft.AI.Foundry.Local.ModelVariant" data-throw-if-not-resolved="false"></xref> is the
the one specific model instance.
All public methods surface consistent error handling through <xref href="Microsoft.AI.Foundry.Local.FoundryLocalException" data-throw-if-not-resolved="false"></xref>.

```csharp
public class ModelVariant : IModel
```

#### Inheritance

[object](https://learn.microsoft.com/dotnet/api/system.object) ← 
[ModelVariant](Microsoft.AI.Foundry.Local.ModelVariant.md)

#### Implements

[IModel](Microsoft.AI.Foundry.Local.IModel.md)

#### Inherited Members

[object.Equals\(object?\)](https://learn.microsoft.com/dotnet/api/system.object.equals\#system\-object\-equals\(system\-object\)), 
[object.Equals\(object?, object?\)](https://learn.microsoft.com/dotnet/api/system.object.equals\#system\-object\-equals\(system\-object\-system\-object\)), 
[object.GetHashCode\(\)](https://learn.microsoft.com/dotnet/api/system.object.gethashcode), 
[object.GetType\(\)](https://learn.microsoft.com/dotnet/api/system.object.gettype), 
[object.MemberwiseClone\(\)](https://learn.microsoft.com/dotnet/api/system.object.memberwiseclone), 
[object.ReferenceEquals\(object?, object?\)](https://learn.microsoft.com/dotnet/api/system.object.referenceequals), 
[object.ToString\(\)](https://learn.microsoft.com/dotnet/api/system.object.tostring)

## Properties

### <a id="Microsoft_AI_Foundry_Local_ModelVariant_Alias"></a> Alias

Alias grouping related variants.

```csharp
public string Alias { get; }
```

#### Property Value

 [string](https://learn.microsoft.com/dotnet/api/system.string)

### <a id="Microsoft_AI_Foundry_Local_ModelVariant_Id"></a> Id

Unique model identifier.

```csharp
public string Id { get; }
```

#### Property Value

 [string](https://learn.microsoft.com/dotnet/api/system.string)

### <a id="Microsoft_AI_Foundry_Local_ModelVariant_Info"></a> Info

Metadata record for this variant.

```csharp
public ModelInfo Info { get; }
```

#### Property Value

 [ModelInfo](Microsoft.AI.Foundry.Local.ModelInfo.md)

### <a id="Microsoft_AI_Foundry_Local_ModelVariant_Version"></a> Version

Parsed version number (falling back to 0 if unavailable).

```csharp
public int Version { get; init; }
```

#### Property Value

 [int](https://learn.microsoft.com/dotnet/api/system.int32)

## Methods

### <a id="Microsoft_AI_Foundry_Local_ModelVariant_DownloadAsync_System_Action_System_Single__System_Nullable_System_Threading_CancellationToken__"></a> DownloadAsync\(Action<float\>?, CancellationToken?\)

Download the model files from the catalog.

```csharp
public Task DownloadAsync(Action<float>? downloadProgress = null, CancellationToken? ct = null)
```

#### Parameters

`downloadProgress` [Action](https://learn.microsoft.com/dotnet/api/system.action\-1)<[float](https://learn.microsoft.com/dotnet/api/system.single)\>?

Optional progress callback called on a separate thread that
    reports download progress in percent (float), with values ending in 100 (percent). When download is complete and all callbacks
    have been made, the Task for the download completes.

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)?

Optional cancellation token.

#### Returns

 [Task](https://learn.microsoft.com/dotnet/api/system.threading.tasks.task)

Task representing the asynchronous operation.

### <a id="Microsoft_AI_Foundry_Local_ModelVariant_GetAudioClientAsync_System_Nullable_System_Threading_CancellationToken__"></a> GetAudioClientAsync\(CancellationToken?\)

Get an OpenAI audio client for the model.

```csharp
public Task<OpenAIAudioClient> GetAudioClientAsync(CancellationToken? ct = null)
```

#### Parameters

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)?

Optional cancellation token.

#### Returns

 [Task](https://learn.microsoft.com/dotnet/api/system.threading.tasks.task\-1)<[OpenAIAudioClient](Microsoft.AI.Foundry.Local.OpenAIAudioClient.md)\>

Task that resolves to an OpenAIAudioClient instance.

### <a id="Microsoft_AI_Foundry_Local_ModelVariant_GetChatClientAsync_System_Nullable_System_Threading_CancellationToken__"></a> GetChatClientAsync\(CancellationToken?\)

Get an OpenAI chat client for the model.

```csharp
public Task<OpenAIChatClient> GetChatClientAsync(CancellationToken? ct = null)
```

#### Parameters

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)?

Optional cancellation token.

#### Returns

 [Task](https://learn.microsoft.com/dotnet/api/system.threading.tasks.task\-1)<[OpenAIChatClient](Microsoft.AI.Foundry.Local.OpenAIChatClient.md)\>

Task that resolves to an OpenAIChatClient instance.

### <a id="Microsoft_AI_Foundry_Local_ModelVariant_GetPathAsync_System_Nullable_System_Threading_CancellationToken__"></a> GetPathAsync\(CancellationToken?\)

Get the file system path where the model is cached.

```csharp
public Task<string> GetPathAsync(CancellationToken? ct = null)
```

#### Parameters

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)?

Optional cancellation token.

#### Returns

 [Task](https://learn.microsoft.com/dotnet/api/system.threading.tasks.task\-1)<[string](https://learn.microsoft.com/dotnet/api/system.string)\>

Task that resolves to the model path string.

### <a id="Microsoft_AI_Foundry_Local_ModelVariant_IsCachedAsync_System_Nullable_System_Threading_CancellationToken__"></a> IsCachedAsync\(CancellationToken?\)

Check if the model is cached on the file system.

```csharp
public Task<bool> IsCachedAsync(CancellationToken? ct = null)
```

#### Parameters

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)?

Optional cancellation token.

#### Returns

 [Task](https://learn.microsoft.com/dotnet/api/system.threading.tasks.task\-1)<[bool](https://learn.microsoft.com/dotnet/api/system.boolean)\>

Task that resolves to true if the model is cached, false otherwise.

### <a id="Microsoft_AI_Foundry_Local_ModelVariant_IsLoadedAsync_System_Nullable_System_Threading_CancellationToken__"></a> IsLoadedAsync\(CancellationToken?\)

Check if the model is currently loaded in the runtime.

```csharp
public Task<bool> IsLoadedAsync(CancellationToken? ct = null)
```

#### Parameters

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)?

Optional cancellation token.

#### Returns

 [Task](https://learn.microsoft.com/dotnet/api/system.threading.tasks.task\-1)<[bool](https://learn.microsoft.com/dotnet/api/system.boolean)\>

Task that resolves to true if the model is loaded, false otherwise.

### <a id="Microsoft_AI_Foundry_Local_ModelVariant_LoadAsync_System_Nullable_System_Threading_CancellationToken__"></a> LoadAsync\(CancellationToken?\)

Load the model so it is available for inferencing.

```csharp
public Task LoadAsync(CancellationToken? ct = null)
```

#### Parameters

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)?

Optional cancellation token.

#### Returns

 [Task](https://learn.microsoft.com/dotnet/api/system.threading.tasks.task)

Task representing the asynchronous operation.

### <a id="Microsoft_AI_Foundry_Local_ModelVariant_RemoveFromCacheAsync_System_Nullable_System_Threading_CancellationToken__"></a> RemoveFromCacheAsync\(CancellationToken?\)

Remove the model files from the cache.

```csharp
public Task RemoveFromCacheAsync(CancellationToken? ct = null)
```

#### Parameters

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)?

Optional cancellation token.

#### Returns

 [Task](https://learn.microsoft.com/dotnet/api/system.threading.tasks.task)

Task representing the asynchronous operation.

### <a id="Microsoft_AI_Foundry_Local_ModelVariant_UnloadAsync_System_Nullable_System_Threading_CancellationToken__"></a> UnloadAsync\(CancellationToken?\)

Unload the model from the runtime.

```csharp
public Task UnloadAsync(CancellationToken? ct = null)
```

#### Parameters

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)?

Optional cancellation token.

#### Returns

 [Task](https://learn.microsoft.com/dotnet/api/system.threading.tasks.task)

Task representing the asynchronous operation.

