[**@prathikrao/foundry-local-sdk**](../README.md)

***

[@prathikrao/foundry-local-sdk](../globals.md) / AudioClient

# Class: AudioClient

Defined in: [openai/audioClient.ts:40](https://github.com/microsoft/Foundry-Local/blob/a42a7bf2423d2b1da6cca82531f4977f139aef46/sdk_v2/js/src/openai/audioClient.ts#L40)

Client for performing audio operations (transcription, translation) with a loaded model.
Follows the OpenAI Audio API structure.

## Constructors

### Constructor

> **new AudioClient**(`modelId`, `coreInterop`): `AudioClient`

Defined in: [openai/audioClient.ts:54](https://github.com/microsoft/Foundry-Local/blob/a42a7bf2423d2b1da6cca82531f4977f139aef46/sdk_v2/js/src/openai/audioClient.ts#L54)

**`Internal`**

Restricted to internal use because CoreInterop is an internal implementation detail.
Users should create clients via the Model.createAudioClient() factory method.

#### Parameters

##### modelId

`string`

##### coreInterop

[`CoreInterop`](CoreInterop.md)

#### Returns

`AudioClient`

## Properties

### settings

> **settings**: [`AudioClientSettings`](AudioClientSettings.md)

Defined in: [openai/audioClient.ts:47](https://github.com/microsoft/Foundry-Local/blob/a42a7bf2423d2b1da6cca82531f4977f139aef46/sdk_v2/js/src/openai/audioClient.ts#L47)

Configuration settings for audio operations.

## Methods

### transcribe()

> **transcribe**(`audioFilePath`): `Promise`\<`any`\>

Defined in: [openai/audioClient.ts:75](https://github.com/microsoft/Foundry-Local/blob/a42a7bf2423d2b1da6cca82531f4977f139aef46/sdk_v2/js/src/openai/audioClient.ts#L75)

Transcribes audio into the input language.

#### Parameters

##### audioFilePath

`string`

Path to the audio file to transcribe.

#### Returns

`Promise`\<`any`\>

The transcription result.

#### Throws

Error - If audioFilePath is invalid or transcription fails.

***

### transcribeStreaming()

> **transcribeStreaming**(`audioFilePath`, `callback`): `Promise`\<`void`\>

Defined in: [openai/audioClient.ts:98](https://github.com/microsoft/Foundry-Local/blob/a42a7bf2423d2b1da6cca82531f4977f139aef46/sdk_v2/js/src/openai/audioClient.ts#L98)

Transcribes audio into the input language using streaming.

#### Parameters

##### audioFilePath

`string`

Path to the audio file to transcribe.

##### callback

(`chunk`) => `void`

A callback function that receives each chunk of the streaming response.

#### Returns

`Promise`\<`void`\>

A promise that resolves when the stream is complete.

#### Throws

Error - If audioFilePath or callback are invalid, or streaming fails.
