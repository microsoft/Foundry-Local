[@prathikrao/foundry-local-sdk](../README.md) / AudioClient

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

#### Overload 1: Async iterable pattern (recommended)

```ts
transcribeStreaming(audioFilePath): AsyncIterable<any>;
```

Transcribes audio into the input language using streaming, returning an async iterable:

```ts
for await (const chunk of audioClient.transcribeStreaming(audioFilePath)) {
  process.stdout.write(chunk.text ?? '');
}
```

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `audioFilePath` | `string` | Path to the audio file to transcribe. |

#### Returns

`AsyncIterable`\<`any`\>

An async iterable that yields each chunk of the streaming response.

#### Throws

Error - If audioFilePath is invalid, or streaming fails.

***

#### Overload 2: Callback pattern

```ts
transcribeStreaming(audioFilePath, callback): Promise<void>;
```

Transcribes audio into the input language using streaming with a callback:

```ts
await audioClient.transcribeStreaming(audioFilePath, (chunk) => {
  process.stdout.write(chunk.text ?? '');
});
```

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `audioFilePath` | `string` | Path to the audio file to transcribe. |
| `callback` | (`chunk`) => `void` | A callback function that receives each chunk of the streaming response. |

#### Returns

`Promise`\<`void`\>

A promise that resolves when the stream is complete.

#### Throws

Error - If audioFilePath or callback are invalid, or streaming fails.
