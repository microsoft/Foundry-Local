# EpInfo

Namespace: Microsoft.AI.Foundry.Local

Describes a discoverable execution provider bootstrapper.

```csharp
public record EpInfo
```

## Properties

### **Name**

The identifier of the bootstrapper/execution provider (e.g. "CUDAExecutionProvider").

```csharp
public string Name { get; set; }
```

#### Property Value

[String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

### **IsRegistered**

True if this EP has already been successfully downloaded and registered.

```csharp
public bool IsRegistered { get; set; }
```

#### Property Value

[Boolean](https://docs.microsoft.com/en-us/dotnet/api/system.boolean)<br>
