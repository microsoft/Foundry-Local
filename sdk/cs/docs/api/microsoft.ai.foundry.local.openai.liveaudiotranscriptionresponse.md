# LiveAudioTranscriptionResponse

Namespace: Microsoft.AI.Foundry.Local.OpenAI

Transcription result for real-time audio streaming sessions.
 Extends the OpenAI Realtime API's  so that
 customers access text via `result.Content[0].Text` or
 `result.Content[0].Transcript`, ensuring forward compatibility
 when the transport layer moves to WebSocket.

```csharp
public class LiveAudioTranscriptionResponse : Betalgo.Ranul.OpenAI.ObjectModels.RealtimeModels.ConversationItem
```

Inheritance [Object](https://docs.microsoft.com/en-us/dotnet/api/system.object) → ConversationItem → [LiveAudioTranscriptionResponse](./microsoft.ai.foundry.local.openai.liveaudiotranscriptionresponse.md)

## Properties

### **IsFinal**

Whether this is a final or partial (interim) result.
 - Nemotron models always return `true` (every result is final).
 - Other models (e.g., Azure Embedded) may return `false` for interim
 hypotheses that will be replaced by a subsequent final result.

```csharp
public bool IsFinal { get; set; }
```

#### Property Value

[Boolean](https://docs.microsoft.com/en-us/dotnet/api/system.boolean)<br>

### **StartTime**

Start time offset of this segment in the audio stream (seconds).

```csharp
public Nullable<double> StartTime { get; set; }
```

#### Property Value

[Nullable&lt;Double&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>

### **EndTime**

End time offset of this segment in the audio stream (seconds).

```csharp
public Nullable<double> EndTime { get; set; }
```

#### Property Value

[Nullable&lt;Double&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>

### **Id**

```csharp
public string Id { get; set; }
```

#### Property Value

[String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

### **Type**

```csharp
public ItemType Type { get; set; }
```

#### Property Value

ItemType<br>

### **Status**

```csharp
public Nullable<Status> Status { get; set; }
```

#### Property Value

[Nullable&lt;Status&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>

### **Role**

```csharp
public Nullable<Role> Role { get; set; }
```

#### Property Value

[Nullable&lt;Role&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>

### **Content**

```csharp
public List<ContentPart> Content { get; set; }
```

#### Property Value

[List&lt;ContentPart&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.collections.generic.list-1)<br>

### **CallId**

```csharp
public string CallId { get; set; }
```

#### Property Value

[String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

### **Name**

```csharp
public string Name { get; set; }
```

#### Property Value

[String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

### **Arguments**

```csharp
public string Arguments { get; set; }
```

#### Property Value

[String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

### **Output**

```csharp
public string Output { get; set; }
```

#### Property Value

[String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>

## Constructors

### **LiveAudioTranscriptionResponse()**

```csharp
public LiveAudioTranscriptionResponse()
```
