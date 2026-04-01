# LiveAudioTranscriptionSession

Namespace: Microsoft.AI.Foundry.Local.OpenAI

Session for real-time audio streaming ASR (Automatic Speech Recognition).
 Audio data from a microphone (or other source) is pushed in as PCM chunks,
 and transcription results are returned as an async stream.
 
 Created via [OpenAIAudioClient.CreateLiveTranscriptionSession()](./microsoft.ai.foundry.local.openaiaudioclient.md#createlivetranscriptionsession).
 
 Thread safety: AppendAsync can be called from any thread (including high-frequency
 audio callbacks). Pushes are internally serialized via a bounded channel to prevent
 unbounded memory growth and ensure ordering.

```csharp
public sealed class LiveAudioTranscriptionSession : System.IAsyncDisposable
```

Inheritance [Object](https://docs.microsoft.com/en-us/dotnet/api/system.object) → [LiveAudioTranscriptionSession](./microsoft.ai.foundry.local.openai.liveaudiotranscriptionsession.md)<br>
Implements [IAsyncDisposable](https://docs.microsoft.com/en-us/dotnet/api/system.iasyncdisposable)<br>
Attributes [NullableContextAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.nullablecontextattribute), [NullableAttribute](https://docs.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.nullableattribute)

## Properties

### **Settings**

```csharp
public LiveAudioTranscriptionOptions Settings { get; }
```

#### Property Value

[LiveAudioTranscriptionOptions](./microsoft.ai.foundry.local.openai.liveaudiotranscriptionsession.liveaudiotranscriptionoptions.md)<br>

## Methods

### **StartAsync(CancellationToken)**

Start a real-time audio streaming session.
 Must be called before [LiveAudioTranscriptionSession.AppendAsync(ReadOnlyMemory&lt;Byte&gt;, CancellationToken)](./microsoft.ai.foundry.local.openai.liveaudiotranscriptionsession.md#appendasyncreadonlymemorybyte-cancellationtoken) or [LiveAudioTranscriptionSession.GetTranscriptionStream(CancellationToken)](./microsoft.ai.foundry.local.openai.liveaudiotranscriptionsession.md#gettranscriptionstreamcancellationtoken).
 Settings are frozen after this call.

```csharp
public Task StartAsync(CancellationToken ct)
```

#### Parameters

`ct` [CancellationToken](https://docs.microsoft.com/en-us/dotnet/api/system.threading.cancellationtoken)<br>
Cancellation token.

#### Returns

[Task](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task)<br>

### **AppendAsync(ReadOnlyMemory&lt;Byte&gt;, CancellationToken)**

Push a chunk of raw PCM audio data to the streaming session.
 Can be called from any thread (including audio device callbacks).
 Chunks are internally queued and serialized to the native core.

```csharp
public ValueTask AppendAsync(ReadOnlyMemory<byte> pcmData, CancellationToken ct)
```

#### Parameters

`pcmData` [ReadOnlyMemory&lt;Byte&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.readonlymemory-1)<br>
Raw PCM audio bytes matching the configured format.

`ct` [CancellationToken](https://docs.microsoft.com/en-us/dotnet/api/system.threading.cancellationtoken)<br>
Cancellation token.

#### Returns

[ValueTask](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.valuetask)<br>

### **GetTranscriptionStream(CancellationToken)**

Get the async stream of transcription results.
 Results arrive as the native ASR engine processes audio data.

```csharp
public IAsyncEnumerable<LiveAudioTranscriptionResponse> GetTranscriptionStream(CancellationToken ct)
```

#### Parameters

`ct` [CancellationToken](https://docs.microsoft.com/en-us/dotnet/api/system.threading.cancellationtoken)<br>
Cancellation token.

#### Returns

[IAsyncEnumerable&lt;LiveAudioTranscriptionResponse&gt;](https://docs.microsoft.com/en-us/dotnet/api/system.collections.generic.iasyncenumerable-1)<br>
Async enumerable of transcription results.

### **StopAsync(CancellationToken)**

Signal end-of-audio and stop the streaming session.
 Any remaining buffered audio in the push queue will be drained to native core first.
 Final results are delivered through [LiveAudioTranscriptionSession.GetTranscriptionStream(CancellationToken)](./microsoft.ai.foundry.local.openai.liveaudiotranscriptionsession.md#gettranscriptionstreamcancellationtoken) before it completes.

```csharp
public Task StopAsync(CancellationToken ct)
```

#### Parameters

`ct` [CancellationToken](https://docs.microsoft.com/en-us/dotnet/api/system.threading.cancellationtoken)<br>
Cancellation token.

#### Returns

[Task](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.task)<br>

### **DisposeAsync()**

Dispose the streaming session. Calls [LiveAudioTranscriptionSession.StopAsync(CancellationToken)](./microsoft.ai.foundry.local.openai.liveaudiotranscriptionsession.md#stopasynccancellationtoken) if the session is still active.
 Safe to call multiple times.

```csharp
public ValueTask DisposeAsync()
```

#### Returns

[ValueTask](https://docs.microsoft.com/en-us/dotnet/api/system.threading.tasks.valuetask)<br>
