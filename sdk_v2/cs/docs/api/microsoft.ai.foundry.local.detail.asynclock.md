# AsyncLock

Namespace: Microsoft.AI.Foundry.Local.Detail

```csharp
public sealed class AsyncLock : System.IDisposable
```

Inheritance [Object](https://docs.microsoft.com/en-us/dotnet/api/system.object) → [AsyncLock](./microsoft.ai.foundry.local.detail.asynclock.md)<br>
Implements [IDisposable](https://docs.microsoft.com/en-us/dotnet/api/system.idisposable)<br>
Attributes [NullableContextAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.nullablecontextattribute), [NullableAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.nullableattribute)

## Constructors

### **AsyncLock()**

```csharp
public AsyncLock()
```

## Methods

### **Dispose()**

```csharp
public void Dispose()
```

### **Lock()**

```csharp
public IDisposable Lock()
```

#### Returns

[IDisposable](https://docs.microsoft.com/en-us/dotnet/api/system.idisposable)<br>

### **LockAsync()**

```csharp
public Task<IDisposable> LockAsync()
```

#### Returns

[Task&lt;IDisposable&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>

