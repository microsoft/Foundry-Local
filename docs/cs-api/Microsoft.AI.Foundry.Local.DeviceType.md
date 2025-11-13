# <a id="Microsoft_AI_Foundry_Local_DeviceType"></a> Enum DeviceType

Namespace: [Microsoft.AI.Foundry.Local](Microsoft.AI.Foundry.Local.md)  
Assembly: Microsoft.AI.Foundry.Local.dll  

Device types supported by the runtime for model execution.

```csharp
[JsonConverter(typeof(JsonStringEnumConverter<DeviceType>))]
public enum DeviceType
```

## Fields

`CPU = 1` 

Standard system CPU.



`GPU = 2` 

Discrete or integrated GPU device.



`Invalid = 0` 

Invalid / unspecified device type.



`NPU = 3` 

Neural Processing Unit.



