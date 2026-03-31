# EpInfo

Namespace: Microsoft.AI.Foundry.Local

Describes a discoverable execution provider bootstrapper.

```csharp
public class EpInfo : System.IEquatable`1[[Microsoft.AI.Foundry.Local.EpInfo, Microsoft.AI.Foundry.Local, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null]]
```

Inheritance [Object](https://docs.microsoft.com/en-us/dotnet/api/system.object) → [EpInfo](./microsoft.ai.foundry.local.epinfo.md)<br>
Implements [IEquatable&lt;EpInfo&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.iequatable-1)<br>
Attributes [NullableContextAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.nullablecontextattribute), [NullableAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.nullableattribute), [RequiredMemberAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.requiredmemberattribute)

## Properties

### **EqualityContract**

```csharp
protected Type EqualityContract { get; }
```

#### Property Value

[Type](https://docs.microsoft.com/en-us/dotnet/api/system.type)<br>

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

## Constructors

### **EpInfo(EpInfo)**

```csharp
protected EpInfo(EpInfo original)
```

#### Parameters

`original` [EpInfo](./microsoft.ai.foundry.local.epinfo.md)<br>

### **EpInfo()**

#### Caution

Constructors of types with required members are not supported in this version of your compiler.

---

```csharp
public EpInfo()
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

### **Equals(EpInfo)**

```csharp
public bool Equals(EpInfo other)
```

#### Parameters

`other` [EpInfo](./microsoft.ai.foundry.local.epinfo.md)<br>

#### Returns

[Boolean](https://docs.microsoft.com/en-us/dotnet/api/system.boolean)<br>

### **&lt;Clone&gt;$()**

```csharp
public EpInfo <Clone>$()
```

#### Returns

[EpInfo](./microsoft.ai.foundry.local.epinfo.md)<br>
