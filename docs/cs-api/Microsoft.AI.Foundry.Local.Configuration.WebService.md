# <a id="Microsoft_AI_Foundry_Local_Configuration_WebService"></a> Class Configuration.WebService

Namespace: [Microsoft.AI.Foundry.Local](Microsoft.AI.Foundry.Local.md)  
Assembly: Microsoft.AI.Foundry.Local.dll  

Configuration settings if the optional web service is used.

```csharp
public class Configuration.WebService
```

#### Inheritance

[object](https://learn.microsoft.com/dotnet/api/system.object) ← 
[Configuration.WebService](Microsoft.AI.Foundry.Local.Configuration.WebService.md)

#### Inherited Members

[object.Equals\(object?\)](https://learn.microsoft.com/dotnet/api/system.object.equals\#system\-object\-equals\(system\-object\)), 
[object.Equals\(object?, object?\)](https://learn.microsoft.com/dotnet/api/system.object.equals\#system\-object\-equals\(system\-object\-system\-object\)), 
[object.GetHashCode\(\)](https://learn.microsoft.com/dotnet/api/system.object.gethashcode), 
[object.GetType\(\)](https://learn.microsoft.com/dotnet/api/system.object.gettype), 
[object.MemberwiseClone\(\)](https://learn.microsoft.com/dotnet/api/system.object.memberwiseclone), 
[object.ReferenceEquals\(object?, object?\)](https://learn.microsoft.com/dotnet/api/system.object.referenceequals), 
[object.ToString\(\)](https://learn.microsoft.com/dotnet/api/system.object.tostring)

## Properties

### <a id="Microsoft_AI_Foundry_Local_Configuration_WebService_ExternalUrl"></a> ExternalUrl

If the web service is running in a separate process, it will be accessed using this URI.
Both processes should be using the same version of the SDK. If a random port is assigned when creating
the web service in the external process the actual port must be provided here.

```csharp
public Uri? ExternalUrl { get; init; }
```

#### Property Value

 [Uri](https://learn.microsoft.com/dotnet/api/system.uri)?

### <a id="Microsoft_AI_Foundry_Local_Configuration_WebService_Urls"></a> Urls

Url/s to bind to the web service when <xref href="Microsoft.AI.Foundry.Local.FoundryLocalManager.StartWebServiceAsync(System.Nullable%7bSystem.Threading.CancellationToken%7d)" data-throw-if-not-resolved="false"></xref> is called.
After startup, <xref href="Microsoft.AI.Foundry.Local.FoundryLocalManager.Urls" data-throw-if-not-resolved="false"></xref> will contain the actual URL/s the service is listening on.
Default: 127.0.0.1:0, which binds to a random ephemeral port. Multiple URLs can be specified as a semi-colon separated list.

```csharp
public string? Urls { get; init; }
```

#### Property Value

 [string](https://learn.microsoft.com/dotnet/api/system.string)?

