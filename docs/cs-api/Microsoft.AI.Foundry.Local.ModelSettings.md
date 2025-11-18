# <a id="Microsoft_AI_Foundry_Local_ModelSettings"></a> Class ModelSettings

Namespace: [Microsoft.AI.Foundry.Local](Microsoft.AI.Foundry.Local.md)  
Assembly: Microsoft.AI.Foundry.Local.dll  

Optional settings applied to a model instance (e.g. default parameters).

```csharp
public record ModelSettings : IEquatable<ModelSettings>
```

#### Inheritance

[object](https://learn.microsoft.com/dotnet/api/system.object) ← 
[ModelSettings](Microsoft.AI.Foundry.Local.ModelSettings.md)

#### Implements

[IEquatable<ModelSettings\>](https://learn.microsoft.com/dotnet/api/system.iequatable\-1)

#### Inherited Members

[object.Equals\(object?\)](https://learn.microsoft.com/dotnet/api/system.object.equals\#system\-object\-equals\(system\-object\)), 
[object.Equals\(object?, object?\)](https://learn.microsoft.com/dotnet/api/system.object.equals\#system\-object\-equals\(system\-object\-system\-object\)), 
[object.GetHashCode\(\)](https://learn.microsoft.com/dotnet/api/system.object.gethashcode), 
[object.GetType\(\)](https://learn.microsoft.com/dotnet/api/system.object.gettype), 
[object.MemberwiseClone\(\)](https://learn.microsoft.com/dotnet/api/system.object.memberwiseclone), 
[object.ReferenceEquals\(object?, object?\)](https://learn.microsoft.com/dotnet/api/system.object.referenceequals), 
[object.ToString\(\)](https://learn.microsoft.com/dotnet/api/system.object.tostring)

## Properties

### <a id="Microsoft_AI_Foundry_Local_ModelSettings_Parameters"></a> Parameters

Collection of parameters for the model or null if none are defined.

```csharp
[JsonPropertyName("parameters")]
public Parameter[]? Parameters { get; set; }
```

#### Property Value

 [Parameter](Microsoft.AI.Foundry.Local.Parameter.md)\[\]?

