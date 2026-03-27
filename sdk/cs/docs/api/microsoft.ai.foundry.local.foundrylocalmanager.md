# FoundryLocalManager

Namespace: Microsoft.AI.Foundry.Local

```csharp
public class FoundryLocalManager : System.IDisposable
```

Inheritance [Object](https://docs.microsoft.com/en-us/dotnet/api/system.object) → [FoundryLocalManager](./microsoft.ai.foundry.local.foundrylocalmanager.md)<br>
Implements [IDisposable](https://docs.microsoft.com/en-us/dotnet/api/system.idisposable)<br>
Attributes [NullableContextAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.nullablecontextattribute), [NullableAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.nullableattribute)

## Properties

### **IsInitialized**

```csharp
public static bool IsInitialized { get; }
```

#### Property Value

[Boolean](https://docs.microsoft.com/en-us/dotnet/api/system.boolean)<br>

### **Instance**

```csharp
public static FoundryLocalManager Instance { get; }
```

#### Property Value

[FoundryLocalManager](./microsoft.ai.foundry.local.foundrylocalmanager.md)<br>

### **Urls**

Bound Urls if the web service has been started. Null otherwise.
 See [FoundryLocalManager.StartWebServiceAsync(Nullable&lt;CancellationToken&gt;)](./microsoft.ai.foundry.local.foundrylocalmanager.md#startwebserviceasyncnullablecancellationtoken).

```csharp
public String[] Urls { get; private set; }
```

#### Property Value

[String[]](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

## Methods

### **CreateAsync(Configuration, ILogger, Nullable&lt;CancellationToken&gt;)**

Create the [FoundryLocalManager](./microsoft.ai.foundry.local.foundrylocalmanager.md) singleton instance.

```csharp
public static Task CreateAsync(Configuration configuration, ILogger logger, Nullable<CancellationToken> ct)
```

#### Parameters

`configuration` [Configuration](./microsoft.ai.foundry.local.configuration.md)<br>
Configuration to use.

`logger` ILogger<br>
Application logger to use.
 Use Microsoft.Extensions.Logging.NullLogger.Instance if you wish to ignore log output from the SDK.

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>
Optional cancellation token for the initialization.

#### Returns

[Task](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task)<br>
Task creating the instance.

#### Exceptions

[FoundryLocalException](./microsoft.ai.foundry.local.foundrylocalexception.md)<br>

### **GetCatalogAsync(Nullable&lt;CancellationToken&gt;)**

Get the model catalog instance.

```csharp
public Task<ICatalog> GetCatalogAsync(Nullable<CancellationToken> ct)
```

#### Parameters

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>
Optional cancellation token.

#### Returns

[Task&lt;ICatalog&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>
The model catalog.

**Remarks:**

The catalog is populated on first use.
 Catalog access no longer blocks on execution provider downloads.
 Call [FoundryLocalManager.DownloadAndRegisterEpsAsync](./microsoft.ai.foundry.local.foundrylocalmanager.md#downloadandregisterepsasync) to explicitly download and register execution providers when hardware acceleration is needed.

### **StartWebServiceAsync(Nullable&lt;CancellationToken&gt;)**

Start the optional web service. This will provide an OpenAI-compatible REST endpoint that supports
 /v1/chat_completions
 /v1/models to list downloaded models
 /v1/models/{model_id} to get model details
 
 [FoundryLocalManager.Urls](./microsoft.ai.foundry.local.foundrylocalmanager.md#urls) is populated with the actual bound Urls after startup.

```csharp
public Task StartWebServiceAsync(Nullable<CancellationToken> ct)
```

#### Parameters

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>
Optional cancellation token.

#### Returns

[Task](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task)<br>
Task starting the web service.

### **StopWebServiceAsync(Nullable&lt;CancellationToken&gt;)**

Stops the web service if started.

```csharp
public Task StopWebServiceAsync(Nullable<CancellationToken> ct)
```

#### Parameters

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>
Optional cancellation token.

#### Returns

[Task](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task)<br>
Task stopping the web service.

### **DiscoverEps()**

Discover available execution providers and their registration status.

```csharp
public EpInfo[] DiscoverEps()
```

#### Returns

`EpInfo[]`<br>
An array of `EpInfo` objects, each with `Name` and `IsRegistered` properties.

### **DownloadAndRegisterEpsAsync(IEnumerable&lt;string&gt;?, Nullable&lt;CancellationToken&gt;)**

Download and register execution providers. This is a blocking call.

```csharp
public Task<EpDownloadResult> DownloadAndRegisterEpsAsync(IEnumerable<string>? names = null, CancellationToken? ct = null)
```

#### Parameters

`names` [IEnumerable&lt;string&gt;?](https://docs.microsoft.com/en-us/dotnet/api/system.collections.generic.ienumerable-1)<br>
Optional list of EP names to download. If null or empty, all available EPs are downloaded.

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>
Optional cancellation token.

#### Returns

[Task&lt;EpDownloadResult&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>
An `EpDownloadResult` with `Success`, `Status`, `RegisteredEps`, and `FailedEps` properties.

### **Dispose()**

```csharp
public void Dispose()
```
