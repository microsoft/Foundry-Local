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

The catalog is populated on first use and returns models based on currently available execution providers.
 To ensure all hardware-accelerated models are listed, call [FoundryLocalManager.DownloadAndRegisterEpsAsync(IEnumerable&lt;String&gt;, Nullable&lt;CancellationToken&gt;)](./microsoft.ai.foundry.local.foundrylocalmanager.md#downloadandregisterepsasyncienumerablestring-nullablecancellationtoken) first to
 register execution providers, then access the catalog.

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

Discovers all available execution provider bootstrappers.
 Returns metadata about each EP including whether it is already registered.

```csharp
public EpInfo[] DiscoverEps()
```

#### Returns

[EpInfo[]](./microsoft.ai.foundry.local.epinfo.md)<br>
Array of EP bootstrapper info describing available EPs.

### **DownloadAndRegisterEpsAsync(IEnumerable&lt;String&gt;, Nullable&lt;CancellationToken&gt;)**

Downloads and registers execution providers. This is a blocking call that completes when all
 requested EPs have been processed.

```csharp
public Task<EpDownloadResult> DownloadAndRegisterEpsAsync(IEnumerable<string> names, Nullable<CancellationToken> ct)
```

#### Parameters

`names` [IEnumerable&lt;String&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.collections.generic.ienumerable-1)<br>
Optional subset of EP bootstrapper names to download (as returned by [FoundryLocalManager.DiscoverEps()](./microsoft.ai.foundry.local.foundrylocalmanager.md#discovereps)).
 If null or empty, all discoverable EPs are downloaded.

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>
Optional cancellation token.

#### Returns

[Task&lt;EpDownloadResult&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>
Result describing which EPs succeeded and which failed.

**Remarks:**

Catalog and model requests use whatever EPs are currently registered and do not block on EP downloads.
 After downloading new EPs, re-fetch the model catalog to include models requiring the newly registered EPs.

### **DownloadAndRegisterEpsAsync(IEnumerable&lt;String&gt;, Action&lt;String, Double&gt;, Nullable&lt;CancellationToken&gt;)**

Downloads and registers execution providers with per-EP progress reporting.

```csharp
public Task<EpDownloadResult> DownloadAndRegisterEpsAsync(IEnumerable<string> names, Action<string, double> progressCallback, Nullable<CancellationToken> ct)
```

#### Parameters

`names` [IEnumerable&lt;String&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.collections.generic.ienumerable-1)<br>
Optional subset of EP bootstrapper names to download (as returned by [FoundryLocalManager.DiscoverEps()](./microsoft.ai.foundry.local.foundrylocalmanager.md#discovereps)).
 If null or empty, all discoverable EPs are downloaded.

`progressCallback` [Action&lt;String, Double&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.action-2)<br>
Callback invoked as each EP downloads. Parameters are (epName, percentComplete) where percentComplete is 0-100.

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>
Optional cancellation token.

#### Returns

[Task&lt;EpDownloadResult&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>
Result describing which EPs succeeded and which failed.

### **Dispose(Boolean)**

```csharp
protected void Dispose(bool disposing)
```

#### Parameters

`disposing` [Boolean](https://docs.microsoft.com/en-us/dotnet/api/system.boolean)<br>

### **Dispose()**

```csharp
public void Dispose()
```
