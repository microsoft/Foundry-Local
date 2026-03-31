# EpInfo

Namespace: Microsoft.AI.Foundry.Local

Information about a discoverable execution provider.

```csharp
public class EpInfo
```

Inheritance [Object](https://docs.microsoft.com/en-us/dotnet/api/system.object) → [EpInfo](./microsoft.ai.foundry.local.epinfo.md)<br>
Attributes [NullableContextAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.nullablecontextattribute), [NullableAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.nullableattribute)

## Properties

### **Name**

The name of the execution provider (e.g. "CUDAExecutionProvider").

```csharp
public string Name { get; set; }
```

#### Property Value

[String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

### **IsRegistered**

Whether the execution provider is currently registered.

```csharp
public bool IsRegistered { get; set; }
```

#### Property Value

[Boolean](https://docs.microsoft.com/en-us/dotnet/api/system.boolean)<br>

## Constructors

### **EpInfo()**

```csharp
public EpInfo()
```
