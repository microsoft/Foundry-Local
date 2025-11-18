# <a id="Microsoft_AI_Foundry_Local_Runtime"></a> Class Runtime

Namespace: [Microsoft.AI.Foundry.Local](Microsoft.AI.Foundry.Local.md)  
Assembly: Microsoft.AI.Foundry.Local.dll  

Runtime configuration details describing how the model will execute.

```csharp
public record Runtime : IEquatable<Runtime>
```

#### Inheritance

[object](https://learn.microsoft.com/dotnet/api/system.object) ← 
[Runtime](Microsoft.AI.Foundry.Local.Runtime.md)

#### Implements

[IEquatable<Runtime\>](https://learn.microsoft.com/dotnet/api/system.iequatable\-1)

#### Inherited Members

[object.Equals\(object?\)](https://learn.microsoft.com/dotnet/api/system.object.equals\#system\-object\-equals\(system\-object\)), 
[object.Equals\(object?, object?\)](https://learn.microsoft.com/dotnet/api/system.object.equals\#system\-object\-equals\(system\-object\-system\-object\)), 
[object.GetHashCode\(\)](https://learn.microsoft.com/dotnet/api/system.object.gethashcode), 
[object.GetType\(\)](https://learn.microsoft.com/dotnet/api/system.object.gettype), 
[object.MemberwiseClone\(\)](https://learn.microsoft.com/dotnet/api/system.object.memberwiseclone), 
[object.ReferenceEquals\(object?, object?\)](https://learn.microsoft.com/dotnet/api/system.object.referenceequals), 
[object.ToString\(\)](https://learn.microsoft.com/dotnet/api/system.object.tostring)

## Properties

### <a id="Microsoft_AI_Foundry_Local_Runtime_DeviceType"></a> DeviceType

Device type the model will run on (e.g. CPU, GPU, NPU).

```csharp
[JsonPropertyName("deviceType")]
public DeviceType DeviceType { get; init; }
```

#### Property Value

 [DeviceType](Microsoft.AI.Foundry.Local.DeviceType.md)

### <a id="Microsoft_AI_Foundry_Local_Runtime_ExecutionProvider"></a> ExecutionProvider

Execution provider name (e.g. QNNExecutionProvider, CUDAExecutionProvider, WebGPUExecutionProvider, etc). Open‑ended string.

```csharp
[JsonPropertyName("executionProvider")]
public string ExecutionProvider { get; init; }
```

#### Property Value

 [string](https://learn.microsoft.com/dotnet/api/system.string)

