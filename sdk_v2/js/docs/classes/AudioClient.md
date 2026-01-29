[**@prathikrao/foundry-local-sdk**](../README.md)

***

[@prathikrao/foundry-local-sdk](../globals.md) / AudioClient

# Class: AudioClient

Defined in: [openai/audioClient.ts:40](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/openai/audioClient.ts#L40)

Client for performing audio operations (transcription, translation) with a loaded model.
Follows the OpenAI Audio API structure.

## Constructors

### Constructor

> **new AudioClient**(`modelId`, `coreInterop`): `AudioClient`

Defined in: [openai/audioClient.ts:50](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/openai/audioClient.ts#L50)

**`Internal`**

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

Defined in: [openai/audioClient.ts:47](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/openai/audioClient.ts#L47)

Configuration settings for audio operations.

## Methods

### transcribe()

> **transcribe**(`audioFilePath`): `Promise`\<`any`\>

Defined in: [openai/audioClient.ts:60](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/openai/audioClient.ts#L60)

Transcribes audio into the input language.

#### Parameters

##### audioFilePath

`string`

Path to the audio file to transcribe.

#### Returns

`Promise`\<`any`\>

The transcription result.

***

### transcribeStreaming()

> **transcribeStreaming**(`audioFilePath`, `callback`): `Promise`\<`void`\>

Defined in: [openai/audioClient.ts:77](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/openai/audioClient.ts#L77)

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
