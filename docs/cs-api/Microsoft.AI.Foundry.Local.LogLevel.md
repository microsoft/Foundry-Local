# <a id="Microsoft_AI_Foundry_Local_LogLevel"></a> Enum LogLevel

Namespace: [Microsoft.AI.Foundry.Local](Microsoft.AI.Foundry.Local.md)  
Assembly: Microsoft.AI.Foundry.Local.dll  

Logging verbosity levels used by the Foundry Local SDK. These levels align with Serilog (Verbose, Debug, Information, Warning, Error, Fatal)
and differ from Microsoft.Extensions.Logging.LogLevel, which includes Trace, Critical, and None.

```csharp
public enum LogLevel
```

## Fields

`Debug = 1` 

Debug level diagnostic messages.



`Error = 4` 

Recoverable error events.



`Fatal = 5` 

Critical errors indicating severe issues.



`Information = 2` 

Information messages describing normal operations.



`Verbose = 0` 

Highly verbose diagnostic output.



`Warning = 3` 

Warning events indicating potential issues.



