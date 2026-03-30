[foundry-local-sdk](../README.md) / AudioClient

# Class: AudioClient

Client for performing audio operations (transcription, translation) with a loaded model.
Follows the OpenAI Audio API structure.

## Properties

### settings

```ts
settings: AudioClientSettings;
```

Configuration settings for audio operations.

## Methods

### transcribe()

```ts
transcribe(audioFilePath): Promise<any>;
```

Transcribes audio into the input language.

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `audioFilePath` | `string` | Path to the audio file to transcribe. |

#### Returns

`Promise`\<`any`\>

The transcription result.

#### Throws

Error - If audioFilePath is invalid or transcription fails.

***

### transcribeStreaming()

```ts
transcribeStreaming(audioFilePath): AsyncIterable<any>;
```

Transcribes audio into the input language using streaming, returning an async iterable of chunks.

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `audioFilePath` | `string` | Path to the audio file to transcribe. |

#### Returns

`AsyncIterable`\<`any`\>

An async iterable that yields parsed streaming transcription chunks.

#### Throws

Error - If audioFilePath is invalid, or streaming fails.

#### Example

```typescript
for await (const chunk of audioClient.transcribeStreaming('recording.wav')) {
    process.stdout.write(chunk.text);
}
```
