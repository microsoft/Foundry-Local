# <a id="Microsoft_AI_Foundry_Local_OpenAIChatClient_ChatSettings"></a> Class OpenAIChatClient.ChatSettings

Namespace: [Microsoft.AI.Foundry.Local](Microsoft.AI.Foundry.Local.md)  
Assembly: Microsoft.AI.Foundry.Local.dll  

Settings controlling chat completion generation. Only the subset supported by Foundry Local.

```csharp
public record OpenAIChatClient.ChatSettings : IEquatable<OpenAIChatClient.ChatSettings>
```

#### Inheritance

[object](https://learn.microsoft.com/dotnet/api/system.object) ← 
[OpenAIChatClient.ChatSettings](Microsoft.AI.Foundry.Local.OpenAIChatClient.ChatSettings.md)

#### Implements

[IEquatable<OpenAIChatClient.ChatSettings\>](https://learn.microsoft.com/dotnet/api/system.iequatable\-1)

#### Inherited Members

[object.Equals\(object?\)](https://learn.microsoft.com/dotnet/api/system.object.equals\#system\-object\-equals\(system\-object\)), 
[object.Equals\(object?, object?\)](https://learn.microsoft.com/dotnet/api/system.object.equals\#system\-object\-equals\(system\-object\-system\-object\)), 
[object.GetHashCode\(\)](https://learn.microsoft.com/dotnet/api/system.object.gethashcode), 
[object.GetType\(\)](https://learn.microsoft.com/dotnet/api/system.object.gettype), 
[object.MemberwiseClone\(\)](https://learn.microsoft.com/dotnet/api/system.object.memberwiseclone), 
[object.ReferenceEquals\(object?, object?\)](https://learn.microsoft.com/dotnet/api/system.object.referenceequals), 
[object.ToString\(\)](https://learn.microsoft.com/dotnet/api/system.object.tostring)

## Properties

### <a id="Microsoft_AI_Foundry_Local_OpenAIChatClient_ChatSettings_FrequencyPenalty"></a> FrequencyPenalty

Penalizes repeated tokens.

```csharp
public float? FrequencyPenalty { get; set; }
```

#### Property Value

 [float](https://learn.microsoft.com/dotnet/api/system.single)?

### <a id="Microsoft_AI_Foundry_Local_OpenAIChatClient_ChatSettings_MaxTokens"></a> MaxTokens

Maximum number of output tokens to generate.

```csharp
public int? MaxTokens { get; set; }
```

#### Property Value

 [int](https://learn.microsoft.com/dotnet/api/system.int32)?

### <a id="Microsoft_AI_Foundry_Local_OpenAIChatClient_ChatSettings_N"></a> N

Number of parallel completions to request.

```csharp
public int? N { get; set; }
```

#### Property Value

 [int](https://learn.microsoft.com/dotnet/api/system.int32)?

### <a id="Microsoft_AI_Foundry_Local_OpenAIChatClient_ChatSettings_PresencePenalty"></a> PresencePenalty

Penalizes new tokens based on whether they appear in the existing text.

```csharp
public float? PresencePenalty { get; set; }
```

#### Property Value

 [float](https://learn.microsoft.com/dotnet/api/system.single)?

### <a id="Microsoft_AI_Foundry_Local_OpenAIChatClient_ChatSettings_RandomSeed"></a> RandomSeed

Optional random seed for deterministic sampling.

```csharp
public int? RandomSeed { get; set; }
```

#### Property Value

 [int](https://learn.microsoft.com/dotnet/api/system.int32)?

### <a id="Microsoft_AI_Foundry_Local_OpenAIChatClient_ChatSettings_Temperature"></a> Temperature

Sampling temperature. Higher values increase randomness.

```csharp
public float? Temperature { get; set; }
```

#### Property Value

 [float](https://learn.microsoft.com/dotnet/api/system.single)?

### <a id="Microsoft_AI_Foundry_Local_OpenAIChatClient_ChatSettings_TopK"></a> TopK

Top-K sampling parameter.

```csharp
public int? TopK { get; set; }
```

#### Property Value

 [int](https://learn.microsoft.com/dotnet/api/system.int32)?

### <a id="Microsoft_AI_Foundry_Local_OpenAIChatClient_ChatSettings_TopP"></a> TopP

Top-P (nucleus) sampling parameter.

```csharp
public float? TopP { get; set; }
```

#### Property Value

 [float](https://learn.microsoft.com/dotnet/api/system.single)?

