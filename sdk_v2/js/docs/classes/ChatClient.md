[**@prathikrao/foundry-local-sdk**](../README.md)

***

[@prathikrao/foundry-local-sdk](../globals.md) / ChatClient

# Class: ChatClient

Defined in: [openai/chatClient.ts:50](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/openai/chatClient.ts#L50)

Client for performing chat completions with a loaded model.
Follows the OpenAI Chat Completion API structure.

## Constructors

### Constructor

> **new ChatClient**(`modelId`, `coreInterop`): `ChatClient`

Defined in: [openai/chatClient.ts:60](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/openai/chatClient.ts#L60)

**`Internal`**

#### Parameters

##### modelId

`string`

##### coreInterop

[`CoreInterop`](CoreInterop.md)

#### Returns

`ChatClient`

## Properties

### settings

> **settings**: [`ChatClientSettings`](ChatClientSettings.md)

Defined in: [openai/chatClient.ts:57](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/openai/chatClient.ts#L57)

Configuration settings for chat completions.

## Methods

### completeChat()

> **completeChat**(`messages`): `Promise`\<`any`\>

Defined in: [openai/chatClient.ts:70](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/openai/chatClient.ts#L70)

Performs a synchronous chat completion.

#### Parameters

##### messages

`any`[]

An array of message objects (e.g., { role: 'user', content: 'Hello' }).

#### Returns

`Promise`\<`any`\>

The chat completion response object.

***

### completeStreamingChat()

> **completeStreamingChat**(`messages`, `callback`): `Promise`\<`void`\>

Defined in: [openai/chatClient.ts:88](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/openai/chatClient.ts#L88)

Performs a streaming chat completion.

#### Parameters

##### messages

`any`[]

An array of message objects.

##### callback

(`chunk`) => `void`

A callback function that receives each chunk of the streaming response.

#### Returns

`Promise`\<`void`\>

A promise that resolves when the stream is complete.
