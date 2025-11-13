# <a id="Microsoft_AI_Foundry_Local_OpenAIAudioClient"></a> Class OpenAIAudioClient

Namespace: [Microsoft.AI.Foundry.Local](Microsoft.AI.Foundry.Local.md)  
Assembly: Microsoft.AI.Foundry.Local.dll  

Audio transcription client using an OpenAI compatible API surface implemented using Betalgo.Ranul.OpenAI SDK types.
Supports transcription of audio files.

```csharp
public class OpenAIAudioClient
```

#### Inheritance

[object](https://learn.microsoft.com/dotnet/api/system.object) ← 
[OpenAIAudioClient](Microsoft.AI.Foundry.Local.OpenAIAudioClient.md)

#### Inherited Members

[object.Equals\(object?\)](https://learn.microsoft.com/dotnet/api/system.object.equals\#system\-object\-equals\(system\-object\)), 
[object.Equals\(object?, object?\)](https://learn.microsoft.com/dotnet/api/system.object.equals\#system\-object\-equals\(system\-object\-system\-object\)), 
[object.GetHashCode\(\)](https://learn.microsoft.com/dotnet/api/system.object.gethashcode), 
[object.GetType\(\)](https://learn.microsoft.com/dotnet/api/system.object.gettype), 
[object.MemberwiseClone\(\)](https://learn.microsoft.com/dotnet/api/system.object.memberwiseclone), 
[object.ReferenceEquals\(object?, object?\)](https://learn.microsoft.com/dotnet/api/system.object.referenceequals), 
[object.ToString\(\)](https://learn.microsoft.com/dotnet/api/system.object.tostring)

## Methods

### <a id="Microsoft_AI_Foundry_Local_OpenAIAudioClient_TranscribeAudioAsync_System_String_System_Nullable_System_Threading_CancellationToken__"></a> TranscribeAudioAsync\(string, CancellationToken?\)

Transcribe audio from a file.

```csharp
public Task<AudioCreateTranscriptionResponse> TranscribeAudioAsync(string audioFilePath, CancellationToken? ct = null)
```

#### Parameters

`audioFilePath` [string](https://learn.microsoft.com/dotnet/api/system.string)

Path to the file containing audio recording.
Supported formats include mp3, wav and flac.

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)?

Optional cancellation token.

#### Returns

 [Task](https://learn.microsoft.com/dotnet/api/system.threading.tasks.task\-1)<AudioCreateTranscriptionResponse\>

Transcription response.

### <a id="Microsoft_AI_Foundry_Local_OpenAIAudioClient_TranscribeAudioStreamingAsync_System_String_System_Threading_CancellationToken_"></a> TranscribeAudioStreamingAsync\(string, CancellationToken\)

Transcribe audio from a file with streamed output.

```csharp
public IAsyncEnumerable<AudioCreateTranscriptionResponse> TranscribeAudioStreamingAsync(string audioFilePath, CancellationToken ct)
```

#### Parameters

`audioFilePath` [string](https://learn.microsoft.com/dotnet/api/system.string)

Path to the file containing audio recording.
Supported formats depend include mp3, wav and flac.

`ct` [CancellationToken](https://learn.microsoft.com/dotnet/api/system.threading.cancellationtoken)

Cancellation token.

#### Returns

 [IAsyncEnumerable](https://learn.microsoft.com/dotnet/api/system.collections.generic.iasyncenumerable\-1)<AudioCreateTranscriptionResponse\>

An asynchronous enumerable of transcription responses.

