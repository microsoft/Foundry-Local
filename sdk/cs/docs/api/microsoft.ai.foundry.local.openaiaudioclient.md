# OpenAIAudioClient

Namespace: Microsoft.AI.Foundry.Local

Audio Client that uses the OpenAI API.
 Implemented using custom OpenAI-compatible types.

```csharp
public class OpenAIAudioClient
```

Inheritance [Object](https://docs.microsoft.com/en-us/dotnet/api/system.object) → [OpenAIAudioClient](./microsoft.ai.foundry.local.openaiaudioclient.md)<br>
Attributes [NullableContextAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.nullablecontextattribute), [NullableAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.nullableattribute)

## Properties

### **Settings**

Settings to use for audio transcription using this client.

```csharp
public AudioSettings Settings { get; }
```

#### Property Value

[AudioSettings](./microsoft.ai.foundry.local.openaiaudioclient.audiosettings.md)<br>

## Methods

### **TranscribeAudioAsync(String, Nullable&lt;CancellationToken&gt;)**

Transcribe audio from a file.

```csharp
public Task<AudioTranscriptionResponse> TranscribeAudioAsync(string audioFilePath, Nullable<CancellationToken> ct)
```

#### Parameters

`audioFilePath` [String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>
Path to file containing audio recording.
 Supported formats: mp3

`ct` [Nullable&lt;CancellationToken&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.nullable-1)<br>
Optional cancellation token.

#### Returns

[Task&lt;AudioTranscriptionResponse&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task-1)<br>
Transcription response.

### **TranscribeAudioStreamingAsync(String, CancellationToken)**

Transcribe audio from a file with streamed output.

```csharp
public IAsyncEnumerable<AudioTranscriptionResponse> TranscribeAudioStreamingAsync(string audioFilePath, CancellationToken ct)
```

#### Parameters

`audioFilePath` [String](https://docs.microsoft.com/en-us/dotnet/api/system.string)<br>
Path to file containing audio recording.
 Supported formats: mp3

`ct` [CancellationToken](https://docs.microsoft.com/en-us/dotnet/api/system.threading.cancellationtoken)<br>
Cancellation token.

#### Returns

[IAsyncEnumerable&lt;AudioTranscriptionResponse&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.collections.generic.iasyncenumerable-1)<br>
An asynchronous enumerable of transcription responses.

### **CreateLiveTranscriptionSession()**

Create a real-time streaming transcription session.
 Audio data is pushed in as PCM chunks and transcription results are returned as an async stream.

```csharp
public LiveAudioTranscriptionSession CreateLiveTranscriptionSession()
```

#### Returns

[LiveAudioTranscriptionSession](./microsoft.ai.foundry.local.openai.liveaudiotranscriptionsession.md)<br>
A streaming session that must be disposed when done.
