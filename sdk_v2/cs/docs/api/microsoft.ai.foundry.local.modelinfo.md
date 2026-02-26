# ModelInfo

Namespace: Microsoft.AI.Foundry.Local

```csharp
public class ModelInfo : System.IEquatable`1[[Microsoft.AI.Foundry.Local.ModelInfo, Microsoft.AI.Foundry.Local, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null]]
```

Inheritance [Object](https://docs.microsoft.com/en-us/dotnet/api/system.object) → [ModelInfo](./microsoft.ai.foundry.local.modelinfo.md)<br>
Implements [IEquatable&lt;ModelInfo&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.iequatable-1)<br>
Attributes [NullableContextAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.nullablecontextattribute), [NullableAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.nullableattribute), [RequiredMemberAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.requiredmemberattribute)

## Properties

### **Id**

```csharp
public string Id { get; set; }
```

#### Property Value

[String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

### **Name**

```csharp
public string Name { get; set; }
```

#### Property Value

[String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

### **Version**

```csharp
public int Version { get; set; }
```

#### Property Value

[Int32](https://docs.microsoft.com/en-us/dotnet/api/system.int32)<br>

### **Alias**

```csharp
public string Alias { get; set; }
```

#### Property Value

[String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

### **DisplayName**

```csharp
public string DisplayName { get; set; }
```

#### Property Value

[String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

### **ProviderType**

```csharp
public string ProviderType { get; set; }
```

#### Property Value

[String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

### **Uri**

```csharp
public string Uri { get; set; }
```

#### Property Value

[String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

### **ModelType**

```csharp
public string ModelType { get; set; }
```

#### Property Value

[String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

### **PromptTemplate**

```csharp
public PromptTemplate PromptTemplate { get; set; }
```

#### Property Value

[PromptTemplate](./microsoft.ai.foundry.local.prompttemplate.md)<br>

### **Publisher**

```csharp
public string Publisher { get; set; }
```

#### Property Value

[String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

### **ModelSettings**

```csharp
public ModelSettings ModelSettings { get; set; }
```

#### Property Value

[ModelSettings](./microsoft.ai.foundry.local.modelsettings.md)<br>

### **License**

```csharp
public string License { get; set; }
```

#### Property Value

[String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

### **LicenseDescription**

```csharp
public string LicenseDescription { get; set; }
```

#### Property Value

[String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

### **Cached**

```csharp
public bool Cached { get; set; }
```

#### Property Value

[Boolean](https://docs.microsoft.com/en-us/dotnet/api/system.boolean)<br>

### **Task**

```csharp
public string Task { get; set; }
```

#### Property Value

[String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

### **Runtime**

```csharp
public Runtime Runtime { get; set; }
```

#### Property Value

[Runtime](./microsoft.ai.foundry.local.runtime.md)<br>

### **FileSizeMb**

```csharp
public Nullable<int> FileSizeMb { get; set; }
```

#### Property Value

[Nullable&lt;Int32&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>

### **SupportsToolCalling**

```csharp
public Nullable<bool> SupportsToolCalling { get; set; }
```

#### Property Value

[Nullable&lt;Boolean&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>

### **MaxOutputTokens**

```csharp
public Nullable<long> MaxOutputTokens { get; set; }
```

#### Property Value

[Nullable&lt;Int64&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>

### **MinFLVersion**

```csharp
public string MinFLVersion { get; set; }
```

#### Property Value

[String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

### **CreatedAtUnix**

```csharp
public long CreatedAtUnix { get; set; }
```

#### Property Value

[Int64](https://docs.microsoft.com/en-us/dotnet/api/system.int64)<br>

## Constructors

### **ModelInfo()**

#### Caution

Constructors of types with required members are not supported in this version of your compiler.

---

```csharp
public ModelInfo()
```

## Methods

### **ToString()**

```csharp
public string ToString()
```

#### Returns

[String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

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

### **Equals(ModelInfo)**

```csharp
public bool Equals(ModelInfo other)
```

#### Parameters

`other` [ModelInfo](./microsoft.ai.foundry.local.modelinfo.md)<br>

#### Returns

[Boolean](https://docs.microsoft.com/en-us/dotnet/api/system.boolean)<br>

