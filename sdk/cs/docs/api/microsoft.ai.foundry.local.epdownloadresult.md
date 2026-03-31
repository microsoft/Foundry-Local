# EpDownloadResult

Namespace: Microsoft.AI.Foundry.Local

Result of an explicit EP download and registration operation.

```csharp
public class EpDownloadResult : System.IEquatable`1[[Microsoft.AI.Foundry.Local.EpDownloadResult, Microsoft.AI.Foundry.Local, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null]]
```

Inheritance [Object](https://docs.microsoft.com/en-us/dotnet/api/system.object) → [EpDownloadResult](./microsoft.ai.foundry.local.epdownloadresult.md)<br>
Implements [IEquatable&lt;EpDownloadResult&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.iequatable-1)<br>
Attributes [NullableContextAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.nullablecontextattribute), [NullableAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.nullableattribute), [RequiredMemberAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.requiredmemberattribute)

## Properties

### **EqualityContract**

```csharp
protected Type EqualityContract { get; }
```

#### Property Value

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

## Constructors

### **EpDownloadResult(EpDownloadResult)**

```csharp
protected EpDownloadResult(EpDownloadResult original)
```

#### Parameters

`original` [EpDownloadResult](./microsoft.ai.foundry.local.epdownloadresult.md)<br>

### **EpDownloadResult()**

#### Caution

Constructors of types with required members are not supported in this version of your compiler.

---

```csharp
public EpDownloadResult()
```

## Methods

### **ToString()**

```csharp
public string ToString()
```

#### Returns

[String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

### **PrintMembers(StringBuilder)**

```csharp
protected bool PrintMembers(StringBuilder builder)
```

#### Parameters

`builder` [StringBuilder](https://docs.microsoft.com/en-us/dotnet/api/system.text.stringbuilder)<br>

#### Returns

[Boolean](https://docs.microsoft.com/en-us/dotnet/api/system.boolean)<br>

### **GetHashCode()**

```csharp
public int GetHashCode()
```

#### Returns

[Int32](https://docs.microsoft.com/en-us/dotnet/api/system.int32)<br>

### **Equals(Object)**

```csharp
public bool Equals(object obj)
```

#### Parameters

`obj` [Object](https://docs.microsoft.com/en-us/dotnet/api/system.object)<br>

#### Returns

[Boolean](https://docs.microsoft.com/en-us/dotnet/api/system.boolean)<br>

### **Equals(EpDownloadResult)**

```csharp
public bool Equals(EpDownloadResult other)
```

#### Parameters

`other` [EpDownloadResult](./microsoft.ai.foundry.local.epdownloadresult.md)<br>

#### Returns

[Boolean](https://docs.microsoft.com/en-us/dotnet/api/system.boolean)<br>

### **&lt;Clone&gt;$()**

```csharp
public EpDownloadResult <Clone>$()
```

#### Returns

[EpDownloadResult](./microsoft.ai.foundry.local.epdownloadresult.md)<br>
