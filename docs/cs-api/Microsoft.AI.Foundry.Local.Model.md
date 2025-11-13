# <a id="Microsoft_AI_Foundry_Local_Model"></a> Class Model

Namespace: [Microsoft.AI.Foundry.Local](Microsoft.AI.Foundry.Local.md)  
Assembly: Microsoft.AI.Foundry.Local.dll  

Represents a family of related model variants (versions or configurations) that share a common alias.
Acts as a façade over its variants, letting you:
 - enumerate and select a specific variant
 - prefer a locally cached variant automatically
 - resolve the latest version of a given variant
 - download, load, unload, cache removal for the currently selected variant
 - create chat and audio clients for the currently selected variant.
Use <xref href="Microsoft.AI.Foundry.Local.ModelVariant" data-throw-if-not-resolved="false"></xref> when you need per‑variant metadata; use <xref href="Microsoft.AI.Foundry.Local.Model" data-throw-if-not-resolved="false"></xref> when you want alias‑level orchestration.

```csharp
public class Model : IModel
```

#### Inheritance

[object](https://learn.microsoft.com/dotnet/api/system.object) ← 
[Model](Microsoft.AI.Foundry.Local.Model.md)

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

### <a id="Microsoft_AI_Foundry_Local_Model_Alias"></a> Alias

Model alias grouping multiple device-specific variants of the same underlying model.

```csharp
public string Alias { get; init; }
```

#### Property Value

 [string](https://learn.microsoft.com/dotnet/api/system.string)

### <a id="Microsoft_AI_Foundry_Local_Model_Id"></a> Id

Unique Id of the currently selected variant.

```csharp
public string Id { get; }
```

#### Property Value

 [string](https://learn.microsoft.com/dotnet/api/system.string)

### <a id="Microsoft_AI_Foundry_Local_Model_SelectedVariant"></a> SelectedVariant

Currently selected variant used for IModel operations.

```csharp
public ModelVariant SelectedVariant { get; }
```

#### Property Value

 [ModelVariant](Microsoft.AI.Foundry.Local.ModelVariant.md)

### <a id="Microsoft_AI_Foundry_Local_Model_Variants"></a> Variants

All known variants for this model alias.

```csharp
public List<ModelVariant> Variants { get; }
```

#### Property Value

 [List](https://learn.microsoft.com/dotnet/api/system.collections.generic.list\-1)<[ModelVariant](Microsoft.AI.Foundry.Local.ModelVariant.md)\>

## Methods

### <a id="Microsoft_AI_Foundry_Local_Model_DownloadAsync_System_Action_System_Single__System_Nullable_System_Threading_CancellationToken__"></a> DownloadAsync\(Action<float\>?, CancellationToken?\)

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

### <a id="Microsoft_AI_Foundry_Local_Model_GetAudioClientAsync_System_Nullable_System_Threading_CancellationToken__"></a> GetAudioClientAsync\(CancellationToken?\)

Get an OpenAI API based AudioClient.

```csharp
public Task<OpenAIAudioClient> GetAudioClientAsync(CancellationToken? ct = null)
```

#### Parameters

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)?

Optional cancellation token.

#### Returns

 [Task](https://learn.microsoft.com/dotnet/api/system.threading.tasks.task\-1)<[OpenAIAudioClient](Microsoft.AI.Foundry.Local.OpenAIAudioClient.md)\>

<xref href="Microsoft.AI.Foundry.Local.OpenAIAudioClient" data-throw-if-not-resolved="false"></xref> instance.

### <a id="Microsoft_AI_Foundry_Local_Model_GetChatClientAsync_System_Nullable_System_Threading_CancellationToken__"></a> GetChatClientAsync\(CancellationToken?\)

Get an OpenAI API based ChatClient.

```csharp
public Task<OpenAIChatClient> GetChatClientAsync(CancellationToken? ct = null)
```

#### Parameters

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)?

Optional cancellation token.

#### Returns

 [Task](https://learn.microsoft.com/dotnet/api/system.threading.tasks.task\-1)<[OpenAIChatClient](Microsoft.AI.Foundry.Local.OpenAIChatClient.md)\>

<xref href="Microsoft.AI.Foundry.Local.OpenAIChatClient" data-throw-if-not-resolved="false"></xref> instance.

### <a id="Microsoft_AI_Foundry_Local_Model_GetLatestVersion_Microsoft_AI_Foundry_Local_ModelVariant_"></a> GetLatestVersion\(ModelVariant\)

Get the latest version of the specified model variant.

```csharp
public ModelVariant GetLatestVersion(ModelVariant variant)
```

#### Parameters

`variant` [ModelVariant](Microsoft.AI.Foundry.Local.ModelVariant.md)

Model variant.

#### Returns

 [ModelVariant](Microsoft.AI.Foundry.Local.ModelVariant.md)

ModelVariant for latest version. Same as <code class="paramref">variant</code> if that is the latest version.

#### Exceptions

 [FoundryLocalException](Microsoft.AI.Foundry.Local.FoundryLocalException.md)

If variant is not valid for this model.

### <a id="Microsoft_AI_Foundry_Local_Model_GetPathAsync_System_Nullable_System_Threading_CancellationToken__"></a> GetPathAsync\(CancellationToken?\)

Gets the model path if cached.

```csharp
public Task<string> GetPathAsync(CancellationToken? ct = null)
```

#### Parameters

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)?

Optional cancellation token.

#### Returns

 [Task](https://learn.microsoft.com/dotnet/api/system.threading.tasks.task\-1)<[string](https://learn.microsoft.com/dotnet/api/system.string)\>

Path of model directory.

### <a id="Microsoft_AI_Foundry_Local_Model_IsCachedAsync_System_Nullable_System_Threading_CancellationToken__"></a> IsCachedAsync\(CancellationToken?\)

Is the currently selected variant cached locally?

```csharp
public Task<bool> IsCachedAsync(CancellationToken? ct = null)
```

#### Parameters

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)?

#### Returns

 [Task](https://learn.microsoft.com/dotnet/api/system.threading.tasks.task\-1)<[bool](https://learn.microsoft.com/dotnet/api/system.boolean)\>

### <a id="Microsoft_AI_Foundry_Local_Model_IsLoadedAsync_System_Nullable_System_Threading_CancellationToken__"></a> IsLoadedAsync\(CancellationToken?\)

Is the currently selected variant loaded in memory?

```csharp
public Task<bool> IsLoadedAsync(CancellationToken? ct = null)
```

#### Parameters

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)?

#### Returns

 [Task](https://learn.microsoft.com/dotnet/api/system.threading.tasks.task\-1)<[bool](https://learn.microsoft.com/dotnet/api/system.boolean)\>

### <a id="Microsoft_AI_Foundry_Local_Model_LoadAsync_System_Nullable_System_Threading_CancellationToken__"></a> LoadAsync\(CancellationToken?\)

Load the model into memory if not already loaded.

```csharp
public Task LoadAsync(CancellationToken? ct = null)
```

#### Parameters

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)?

Optional cancellation token.

#### Returns

 [Task](https://learn.microsoft.com/dotnet/api/system.threading.tasks.task)

### <a id="Microsoft_AI_Foundry_Local_Model_RemoveFromCacheAsync_System_Nullable_System_Threading_CancellationToken__"></a> RemoveFromCacheAsync\(CancellationToken?\)

Remove the model from the local cache.

```csharp
public Task RemoveFromCacheAsync(CancellationToken? ct = null)
```

#### Parameters

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)?

Optional cancellation token.

#### Returns

 [Task](https://learn.microsoft.com/dotnet/api/system.threading.tasks.task)

### <a id="Microsoft_AI_Foundry_Local_Model_SelectVariant_Microsoft_AI_Foundry_Local_ModelVariant_"></a> SelectVariant\(ModelVariant\)

Select a specific model variant by its unique model ID.
The selected variant will be used for <xref href="Microsoft.AI.Foundry.Local.IModel" data-throw-if-not-resolved="false"></xref> operations.

```csharp
public void SelectVariant(ModelVariant variant)
```

#### Parameters

`variant` [ModelVariant](Microsoft.AI.Foundry.Local.ModelVariant.md)

#### Exceptions

 [FoundryLocalException](Microsoft.AI.Foundry.Local.FoundryLocalException.md)

If variant is not valid for this model.

### <a id="Microsoft_AI_Foundry_Local_Model_UnloadAsync_System_Nullable_System_Threading_CancellationToken__"></a> UnloadAsync\(CancellationToken?\)

Unload the model if loaded.

```csharp
public Task UnloadAsync(CancellationToken? ct = null)
```

#### Parameters

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)?

Optional cancellation token.

#### Returns

 [Task](https://learn.microsoft.com/dotnet/api/system.threading.tasks.task)

