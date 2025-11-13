# <a id="Microsoft_AI_Foundry_Local_PromptTemplate"></a> Class PromptTemplate

Namespace: [Microsoft.AI.Foundry.Local](Microsoft.AI.Foundry.Local.md)  
Assembly: Microsoft.AI.Foundry.Local.dll  

Template segments used to build a prompt for a model.
For AzureFoundry model types you do NOT need to populate this; Foundry Local will handle prompt construction automatically.

```csharp
public record PromptTemplate : IEquatable<PromptTemplate>
```

#### Inheritance

[object](https://learn.microsoft.com/dotnet/api/system.object) ← 
[PromptTemplate](Microsoft.AI.Foundry.Local.PromptTemplate.md)

#### Implements

[IEquatable<PromptTemplate\>](https://learn.microsoft.com/dotnet/api/system.iequatable\-1)

#### Inherited Members

[object.Equals\(object?\)](https://learn.microsoft.com/dotnet/api/system.object.equals\#system\-object\-equals\(system\-object\)), 
[object.Equals\(object?, object?\)](https://learn.microsoft.com/dotnet/api/system.object.equals\#system\-object\-equals\(system\-object\-system\-object\)), 
[object.GetHashCode\(\)](https://learn.microsoft.com/dotnet/api/system.object.gethashcode), 
[object.GetType\(\)](https://learn.microsoft.com/dotnet/api/system.object.gettype), 
[object.MemberwiseClone\(\)](https://learn.microsoft.com/dotnet/api/system.object.memberwiseclone), 
[object.ReferenceEquals\(object?, object?\)](https://learn.microsoft.com/dotnet/api/system.object.referenceequals), 
[object.ToString\(\)](https://learn.microsoft.com/dotnet/api/system.object.tostring)

## Properties

### <a id="Microsoft_AI_Foundry_Local_PromptTemplate_Assistant"></a> Assistant

Assistant response segment used when constructing multi‑turn prompts.

```csharp
[JsonPropertyName("assistant")]
public string Assistant { get; init; }
```

#### Property Value

 [string](https://learn.microsoft.com/dotnet/api/system.string)

### <a id="Microsoft_AI_Foundry_Local_PromptTemplate_Prompt"></a> Prompt

Raw prompt text passed to the model.

```csharp
[JsonPropertyName("prompt")]
public string Prompt { get; init; }
```

#### Property Value

 [string](https://learn.microsoft.com/dotnet/api/system.string)

### <a id="Microsoft_AI_Foundry_Local_PromptTemplate_System"></a> System

Optional system instruction segment.

```csharp
[JsonPropertyName("system")]
public string? System { get; init; }
```

#### Property Value

 [string](https://learn.microsoft.com/dotnet/api/system.string)?

### <a id="Microsoft_AI_Foundry_Local_PromptTemplate_User"></a> User

Optional user message segment.

```csharp
[JsonPropertyName("user")]
public string? User { get; init; }
```

#### Property Value

 [string](https://learn.microsoft.com/dotnet/api/system.string)?

