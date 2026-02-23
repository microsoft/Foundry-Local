# OpenAIAudioClient

Namespace: Microsoft.AI.Foundry.Local

Audio Client that uses the OpenAI API.
 Implemented using Betalgo.Ranul.OpenAI SDK types.

```csharp
public class OpenAIAudioClient
```

Inheritance [Object](https://docs.microsoft.com/en-us/dotnet/api/system.object) → [OpenAIAudioClient](./microsoft.ai.foundry.local.openaiaudioclient.md)<br>
Attributes [NullableContextAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.nullablecontextattribute), [NullableAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.nullableattribute)

## Properties

### **Settings**

Settings to use for chat completions using this client.

```csharp
public AudioSettings Settings { get; }
```

#### Property Value

[AudioSettings](./microsoft.ai.foundry.local.openaiaudioclient.audiosettings.md)<br>

## Methods

### **TranscribeAudioAsync(String, Nullable&lt;CancellationToken&gt;)**

Transcribe audio from a file.

```csharp
public Task<AudioCreateTranscriptionResponse> TranscribeAudioAsync(string audioFilePath, Nullable<CancellationToken> ct)
```

#### Parameters

`audioFilePath` [String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>
Path to file containing audio recording.
 Supported formats: mp3

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>
Optional cancellation token.

#### Returns

[Task&lt;AudioCreateTranscriptionResponse&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>
Transcription response.

### **TranscribeAudioStreamingAsync(String, CancellationToken)**

Transcribe audio from a file with streamed output.

```csharp
public IAsyncEnumerable<AudioCreateTranscriptionResponse> TranscribeAudioStreamingAsync(string audioFilePath, CancellationToken ct)
```

#### Parameters

`audioFilePath` [String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>
Path to file containing audio recording.
 Supported formats: mp3

`ct` [CancellationToken](https://docs.microsoft.com/en-us/dotnet/api/system.threading.cancellationtoken)<br>
Cancellation token.

#### Returns

[IAsyncEnumerable&lt;AudioCreateTranscriptionResponse&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.collections.generic.iasyncenumerable-1)<br>
An asynchronous enumerable of transcription responses.
