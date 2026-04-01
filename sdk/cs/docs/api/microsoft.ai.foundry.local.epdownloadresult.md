# EpDownloadResult

Namespace: Microsoft.AI.Foundry.Local

Result of an explicit EP download and registration operation.

```csharp
public record EpDownloadResult : System.IEquatable`1[[Microsoft.AI.Foundry.Local.EpDownloadResult, Microsoft.AI.Foundry.Local, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null]]
```

## Properties

### Property Value

[Type](https://docs.microsoft.com/en-us/dotnet/api/system.type)<br>

### **Success**

True if all requested EPs were successfully downloaded and registered.

```csharp
public bool Success { get; set; }
```

#### Property Value

[Boolean](https://docs.microsoft.com/en-us/dotnet/api/system.boolean)<br>

### **Status**

Human-readable status message.

```csharp
public string Status { get; set; }
```

#### Property Value

[String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

### **RegisteredEps**

Names of EPs that were successfully registered.

```csharp
public String[] RegisteredEps { get; set; }
```

#### Property Value

[String[]](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

### **FailedEps**

Names of EPs that failed to register.

```csharp
public String[] FailedEps { get; set; }
```

#### Property Value

[String[]](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>
