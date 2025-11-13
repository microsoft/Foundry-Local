# <a id="Microsoft_AI_Foundry_Local_FoundryLocalManager"></a> Class FoundryLocalManager

Namespace: [Microsoft.AI.Foundry.Local](Microsoft.AI.Foundry.Local.md)  
Assembly: Microsoft.AI.Foundry.Local.dll  

Entry point for Foundry Local SDK providing initialization, catalog access, model management
and optional web service hosting.

```csharp
public class FoundryLocalManager : IDisposable
```

#### Inheritance

[object](https://learn.microsoft.com/dotnet/api/system.object) ← 
[FoundryLocalManager](Microsoft.AI.Foundry.Local.FoundryLocalManager.md)

#### Implements

[IDisposable](https://learn.microsoft.com/dotnet/api/system.idisposable)

#### Inherited Members

[object.Equals\(object?\)](https://learn.microsoft.com/dotnet/api/system.object.equals\#system\-object\-equals\(system\-object\)), 
[object.Equals\(object?, object?\)](https://learn.microsoft.com/dotnet/api/system.object.equals\#system\-object\-equals\(system\-object\-system\-object\)), 
[object.GetHashCode\(\)](https://learn.microsoft.com/dotnet/api/system.object.gethashcode), 
[object.GetType\(\)](https://learn.microsoft.com/dotnet/api/system.object.gettype), 
[object.MemberwiseClone\(\)](https://learn.microsoft.com/dotnet/api/system.object.memberwiseclone), 
[object.ReferenceEquals\(object?, object?\)](https://learn.microsoft.com/dotnet/api/system.object.referenceequals), 
[object.ToString\(\)](https://learn.microsoft.com/dotnet/api/system.object.tostring)

## Properties

### <a id="Microsoft_AI_Foundry_Local_FoundryLocalManager_Instance"></a> Instance

Singleton instance. Must call <xref href="Microsoft.AI.Foundry.Local.FoundryLocalManager.CreateAsync(Microsoft.AI.Foundry.Local.Configuration%2cMicrosoft.Extensions.Logging.ILogger%2cSystem.Nullable%7bSystem.Threading.CancellationToken%7d)" data-throw-if-not-resolved="false"></xref> before use.

```csharp
public static FoundryLocalManager Instance { get; }
```

#### Property Value

 [FoundryLocalManager](Microsoft.AI.Foundry.Local.FoundryLocalManager.md)

### <a id="Microsoft_AI_Foundry_Local_FoundryLocalManager_IsInitialized"></a> IsInitialized

Has the manager been successfully initialized?

```csharp
public static bool IsInitialized { get; }
```

#### Property Value

 [bool](https://learn.microsoft.com/dotnet/api/system.boolean)

### <a id="Microsoft_AI_Foundry_Local_FoundryLocalManager_Urls"></a> Urls

Bound Urls if the web service has been started. Null otherwise.
See <xref href="Microsoft.AI.Foundry.Local.FoundryLocalManager.StartWebServiceAsync(System.Nullable%7bSystem.Threading.CancellationToken%7d)" data-throw-if-not-resolved="false"></xref>.

```csharp
public string[]? Urls { get; }
```

#### Property Value

 [string](https://learn.microsoft.com/dotnet/api/system.string)\[\]?

## Methods

### <a id="Microsoft_AI_Foundry_Local_FoundryLocalManager_CreateAsync_Microsoft_AI_Foundry_Local_Configuration_Microsoft_Extensions_Logging_ILogger_System_Nullable_System_Threading_CancellationToken__"></a> CreateAsync\(Configuration, ILogger, CancellationToken?\)

Create the <xref href="Microsoft.AI.Foundry.Local.FoundryLocalManager" data-throw-if-not-resolved="false"></xref> singleton instance.

```csharp
public static Task CreateAsync(Configuration configuration, ILogger logger, CancellationToken? ct = null)
```

#### Parameters

`configuration` [Configuration](Microsoft.AI.Foundry.Local.Configuration.md)

Configuration to use.

`logger` [ILogger](https://learn.microsoft.com/dotnet/api/microsoft.extensions.logging.ilogger)

Application logger to use.

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)?

Optional cancellation token for the initialization.

#### Returns

 [Task](https://learn.microsoft.com/dotnet/api/system.threading.tasks.task)

Task creating the instance.

### <a id="Microsoft_AI_Foundry_Local_FoundryLocalManager_Dispose_System_Boolean_"></a> Dispose\(bool\)

Dispose managed resources held by the manager.

```csharp
protected virtual void Dispose(bool disposing)
```

#### Parameters

`disposing` [bool](https://learn.microsoft.com/dotnet/api/system.boolean)

True when called from <xref href="Microsoft.AI.Foundry.Local.FoundryLocalManager.Dispose(System.Boolean)" data-throw-if-not-resolved="false"></xref>.

### <a id="Microsoft_AI_Foundry_Local_FoundryLocalManager_Dispose"></a> Dispose\(\)

Dispose the manager instance.

```csharp
public void Dispose()
```

### <a id="Microsoft_AI_Foundry_Local_FoundryLocalManager_EnsureEpsDownloadedAsync_System_Nullable_System_Threading_CancellationToken__"></a> EnsureEpsDownloadedAsync\(CancellationToken?\)

Ensure execution providers are downloaded and registered (For Microsoft.AI.Foundry.Local.WinML package).
Subsequent calls are fast after initial download.

```csharp
public Task EnsureEpsDownloadedAsync(CancellationToken? ct = null)
```

#### Parameters

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)?

Optional cancellation token.

#### Returns

 [Task](https://learn.microsoft.com/dotnet/api/system.threading.tasks.task)

### <a id="Microsoft_AI_Foundry_Local_FoundryLocalManager_GetCatalogAsync_System_Nullable_System_Threading_CancellationToken__"></a> GetCatalogAsync\(CancellationToken?\)

Get the model catalog instance. Populated on first use.

```csharp
public Task<ICatalog> GetCatalogAsync(CancellationToken? ct = null)
```

#### Parameters

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)?

Optional cancellation token.

#### Returns

 [Task](https://learn.microsoft.com/dotnet/api/system.threading.tasks.task\-1)<[ICatalog](Microsoft.AI.Foundry.Local.ICatalog.md)\>

The model catalog.

#### Remarks

If using Microsoft.AI.Foundry.Local.WinML this will trigger execution provider download if not already done.
If the execution provider is already downloaded and up-to-date then this operation is fast. You can call
<xref href="Microsoft.AI.Foundry.Local.FoundryLocalManager.EnsureEpsDownloadedAsync(System.Nullable%7bSystem.Threading.CancellationToken%7d)" data-throw-if-not-resolved="false"></xref> first to separate these operations - for example, during
application startup.

### <a id="Microsoft_AI_Foundry_Local_FoundryLocalManager_StartWebServiceAsync_System_Nullable_System_Threading_CancellationToken__"></a> StartWebServiceAsync\(CancellationToken?\)

Start the optional web service exposing OpenAI compatible endpoints that supports:
   /v1/chat/completions
   /v1/audio/transcriptions
   /v1/models
   /v1/models/{model_id}

```csharp
public Task StartWebServiceAsync(CancellationToken? ct = null)
```

#### Parameters

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)?

Optional cancellation token.

#### Returns

 [Task](https://learn.microsoft.com/dotnet/api/system.threading.tasks.task)

Task completing once service started.

### <a id="Microsoft_AI_Foundry_Local_FoundryLocalManager_StopWebServiceAsync_System_Nullable_System_Threading_CancellationToken__"></a> StopWebServiceAsync\(CancellationToken?\)

Stops the web service if started.

```csharp
public Task StopWebServiceAsync(CancellationToken? ct = null)
```

#### Parameters

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)?

Optional cancellation token.

#### Returns

 [Task](https://learn.microsoft.com/dotnet/api/system.threading.tasks.task)

Task completing once service stopped.

