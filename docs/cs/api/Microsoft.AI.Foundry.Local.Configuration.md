# <a id="Microsoft_AI_Foundry_Local_Configuration"></a> Class Configuration

Namespace: [Microsoft.AI.Foundry.Local](Microsoft.AI.Foundry.Local.md)  
Assembly: Microsoft.AI.Foundry.Local.dll  

Foundry Local configuration used to initialize the <xref href="Microsoft.AI.Foundry.Local.FoundryLocalManager" data-throw-if-not-resolved="false"></xref> singleton.

```csharp
public class Configuration
```

#### Inheritance

[object](https://learn.microsoft.com/dotnet/api/system.object) ← 
[Configuration](Microsoft.AI.Foundry.Local.Configuration.md)

#### Inherited Members

[object.Equals\(object?\)](https://learn.microsoft.com/dotnet/api/system.object.equals\#system\-object\-equals\(system\-object\)), 
[object.Equals\(object?, object?\)](https://learn.microsoft.com/dotnet/api/system.object.equals\#system\-object\-equals\(system\-object\-system\-object\)), 
[object.GetHashCode\(\)](https://learn.microsoft.com/dotnet/api/system.object.gethashcode), 
[object.GetType\(\)](https://learn.microsoft.com/dotnet/api/system.object.gettype), 
[object.MemberwiseClone\(\)](https://learn.microsoft.com/dotnet/api/system.object.memberwiseclone), 
[object.ReferenceEquals\(object?, object?\)](https://learn.microsoft.com/dotnet/api/system.object.referenceequals), 
[object.ToString\(\)](https://learn.microsoft.com/dotnet/api/system.object.tostring)

## Properties

### <a id="Microsoft_AI_Foundry_Local_Configuration_AdditionalSettings"></a> AdditionalSettings

Additional settings that Foundry Local Core can consume.
Keys and values are strings.

```csharp
public IDictionary<string, string>? AdditionalSettings { get; init; }
```

#### Property Value

 [IDictionary](https://learn.microsoft.com/dotnet/api/system.collections.generic.idictionary\-2)<[string](https://learn.microsoft.com/dotnet/api/system.string), [string](https://learn.microsoft.com/dotnet/api/system.string)\>?

### <a id="Microsoft_AI_Foundry_Local_Configuration_AppDataDir"></a> AppDataDir

Application data directory.
Default: {home}/.{appname}, where {home} is the user's home directory and {appname} is the AppName value.

```csharp
public string? AppDataDir { get; init; }
```

#### Property Value

 [string](https://learn.microsoft.com/dotnet/api/system.string)?

### <a id="Microsoft_AI_Foundry_Local_Configuration_AppName"></a> AppName

Your application name. MUST be set to a valid name.

```csharp
public required string AppName { get; set; }
```

#### Property Value

 [string](https://learn.microsoft.com/dotnet/api/system.string)

### <a id="Microsoft_AI_Foundry_Local_Configuration_LogLevel"></a> LogLevel

Logging level.
Valid values are: Verbose, Debug, Information, Warning, Error, Fatal.
Default: <xref href="Microsoft.AI.Foundry.Local.LogLevel.Warning" data-throw-if-not-resolved="false"></xref>.

```csharp
public LogLevel LogLevel { get; init; }
```

#### Property Value

 [LogLevel](Microsoft.AI.Foundry.Local.LogLevel.md)

### <a id="Microsoft_AI_Foundry_Local_Configuration_LogsDir"></a> LogsDir

Log directory.
Default: {appdata}/logs

```csharp
public string? LogsDir { get; init; }
```

#### Property Value

 [string](https://learn.microsoft.com/dotnet/api/system.string)?

### <a id="Microsoft_AI_Foundry_Local_Configuration_ModelCacheDir"></a> ModelCacheDir

Model cache directory.
Default: {appdata}/cache/models, where {appdata} is the AppDataDir value.

```csharp
public string? ModelCacheDir { get; init; }
```

#### Property Value

 [string](https://learn.microsoft.com/dotnet/api/system.string)?

### <a id="Microsoft_AI_Foundry_Local_Configuration_Web"></a> Web

Optional configuration for the built-in web service.
NOTE: This is not included in all builds.

```csharp
public Configuration.WebService? Web { get; init; }
```

#### Property Value

 [Configuration](Microsoft.AI.Foundry.Local.Configuration.md).[WebService](Microsoft.AI.Foundry.Local.Configuration.WebService.md)?

