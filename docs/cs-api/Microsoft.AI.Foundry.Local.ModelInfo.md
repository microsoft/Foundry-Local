# <a id="Microsoft_AI_Foundry_Local_ModelInfo"></a> Class ModelInfo

Namespace: [Microsoft.AI.Foundry.Local](Microsoft.AI.Foundry.Local.md)  
Assembly: Microsoft.AI.Foundry.Local.dll  

Full descriptive metadata for a model variant within the catalog.

```csharp
public record ModelInfo : IEquatable<ModelInfo>
```

#### Inheritance

[object](https://learn.microsoft.com/dotnet/api/system.object) ← 
[ModelInfo](Microsoft.AI.Foundry.Local.ModelInfo.md)

#### Implements

[IEquatable<ModelInfo\>](https://learn.microsoft.com/dotnet/api/system.iequatable\-1)

#### Inherited Members

[object.Equals\(object?\)](https://learn.microsoft.com/dotnet/api/system.object.equals\#system\-object\-equals\(system\-object\)), 
[object.Equals\(object?, object?\)](https://learn.microsoft.com/dotnet/api/system.object.equals\#system\-object\-equals\(system\-object\-system\-object\)), 
[object.GetHashCode\(\)](https://learn.microsoft.com/dotnet/api/system.object.gethashcode), 
[object.GetType\(\)](https://learn.microsoft.com/dotnet/api/system.object.gettype), 
[object.MemberwiseClone\(\)](https://learn.microsoft.com/dotnet/api/system.object.memberwiseclone), 
[object.ReferenceEquals\(object?, object?\)](https://learn.microsoft.com/dotnet/api/system.object.referenceequals), 
[object.ToString\(\)](https://learn.microsoft.com/dotnet/api/system.object.tostring)

## Properties

### <a id="Microsoft_AI_Foundry_Local_ModelInfo_Cached"></a> Cached

Indicates whether the model artifacts are currently cached locally.

```csharp
[JsonPropertyName("cached")]
public bool Cached { get; init; }
```

#### Property Value

 [bool](https://learn.microsoft.com/dotnet/api/system.boolean)

### <a id="Microsoft_AI_Foundry_Local_ModelInfo_CreatedAtUnix"></a> CreatedAtUnix

Unix timestamp (seconds) when the model was added to the catalog.

```csharp
[JsonPropertyName("createdAt")]
public long CreatedAtUnix { get; init; }
```

#### Property Value

 [long](https://learn.microsoft.com/dotnet/api/system.int64)

### <a id="Microsoft_AI_Foundry_Local_ModelInfo_DisplayName"></a> DisplayName

Friendly display name.

```csharp
[JsonPropertyName("displayName")]
public string? DisplayName { get; init; }
```

#### Property Value

 [string](https://learn.microsoft.com/dotnet/api/system.string)?

### <a id="Microsoft_AI_Foundry_Local_ModelInfo_FileSizeMb"></a> FileSizeMb

Approximate size of the model artifacts in megabytes.

```csharp
[JsonPropertyName("fileSizeMb")]
public int? FileSizeMb { get; init; }
```

#### Property Value

 [int](https://learn.microsoft.com/dotnet/api/system.int32)?

### <a id="Microsoft_AI_Foundry_Local_ModelInfo_Id"></a> Id

Globally unique model identifier.

```csharp
[JsonPropertyName("id")]
public required string Id { get; init; }
```

#### Property Value

 [string](https://learn.microsoft.com/dotnet/api/system.string)

### <a id="Microsoft_AI_Foundry_Local_ModelInfo_License"></a> License

Short license identifier or name associated with this model.

```csharp
[JsonPropertyName("license")]
public string? License { get; init; }
```

#### Property Value

 [string](https://learn.microsoft.com/dotnet/api/system.string)?

### <a id="Microsoft_AI_Foundry_Local_ModelInfo_LicenseDescription"></a> LicenseDescription

Extended license description, terms, or URL providing more license details.

```csharp
[JsonPropertyName("licenseDescription")]
public string? LicenseDescription { get; init; }
```

#### Property Value

 [string](https://learn.microsoft.com/dotnet/api/system.string)?

### <a id="Microsoft_AI_Foundry_Local_ModelInfo_MaxOutputTokens"></a> MaxOutputTokens

Maximum supported output tokens for generation.

```csharp
[JsonPropertyName("maxOutputTokens")]
public long? MaxOutputTokens { get; init; }
```

#### Property Value

 [long](https://learn.microsoft.com/dotnet/api/system.int64)?

### <a id="Microsoft_AI_Foundry_Local_ModelInfo_MinFLVersion"></a> MinFLVersion

Minimum required Foundry Local CLI version for this model.

```csharp
[JsonPropertyName("minFLVersion")]
public string? MinFLVersion { get; init; }
```

#### Property Value

 [string](https://learn.microsoft.com/dotnet/api/system.string)?

### <a id="Microsoft_AI_Foundry_Local_ModelInfo_ModelSettings"></a> ModelSettings

Optional settings applied to this model variant (e.g. default parameter values).

```csharp
[JsonPropertyName("modelSettings")]
public ModelSettings? ModelSettings { get; init; }
```

#### Property Value

 [ModelSettings](Microsoft.AI.Foundry.Local.ModelSettings.md)?

### <a id="Microsoft_AI_Foundry_Local_ModelInfo_ModelType"></a> ModelType

The model type for example, ONNX.

```csharp
[JsonPropertyName("modelType")]
public required string ModelType { get; init; }
```

#### Property Value

 [string](https://learn.microsoft.com/dotnet/api/system.string)

### <a id="Microsoft_AI_Foundry_Local_ModelInfo_Name"></a> Name

Internal model name (typically includes size / architecture).

```csharp
[JsonPropertyName("name")]
public required string Name { get; init; }
```

#### Property Value

 [string](https://learn.microsoft.com/dotnet/api/system.string)

### <a id="Microsoft_AI_Foundry_Local_ModelInfo_PromptTemplate"></a> PromptTemplate

Optional prompt template guidance for this model.

```csharp
[JsonPropertyName("promptTemplate")]
public PromptTemplate? PromptTemplate { get; init; }
```

#### Property Value

 [PromptTemplate](Microsoft.AI.Foundry.Local.PromptTemplate.md)?

### <a id="Microsoft_AI_Foundry_Local_ModelInfo_ProviderType"></a> ProviderType

Either AzureFoundry (model from Catalog) or Local (model from local filesystem but not found in the catalog).

```csharp
[JsonPropertyName("providerType")]
public required string ProviderType { get; init; }
```

#### Property Value

 [string](https://learn.microsoft.com/dotnet/api/system.string)

### <a id="Microsoft_AI_Foundry_Local_ModelInfo_Publisher"></a> Publisher

Publisher or organization name.

```csharp
[JsonPropertyName("publisher")]
public string? Publisher { get; init; }
```

#### Property Value

 [string](https://learn.microsoft.com/dotnet/api/system.string)?

### <a id="Microsoft_AI_Foundry_Local_ModelInfo_Runtime"></a> Runtime

Runtime configuration details (device, execution provider) for executing the model.

```csharp
[JsonPropertyName("runtime")]
public Runtime? Runtime { get; init; }
```

#### Property Value

 [Runtime](Microsoft.AI.Foundry.Local.Runtime.md)?

### <a id="Microsoft_AI_Foundry_Local_ModelInfo_SupportsToolCalling"></a> SupportsToolCalling

Indicates if the model supports tool/function calling capabilities.

```csharp
[JsonPropertyName("supportsToolCalling")]
public bool? SupportsToolCalling { get; init; }
```

#### Property Value

 [bool](https://learn.microsoft.com/dotnet/api/system.boolean)?

### <a id="Microsoft_AI_Foundry_Local_ModelInfo_Task"></a> Task

Primary task this model is intended for (e.g. text-generation, embeddings, speech-to-text).

```csharp
[JsonPropertyName("task")]
public string? Task { get; init; }
```

#### Property Value

 [string](https://learn.microsoft.com/dotnet/api/system.string)?

### <a id="Microsoft_AI_Foundry_Local_ModelInfo_Uri"></a> Uri

Source URI for the model artifacts.

```csharp
[JsonPropertyName("uri")]
public required string Uri { get; init; }
```

#### Property Value

 [string](https://learn.microsoft.com/dotnet/api/system.string)

