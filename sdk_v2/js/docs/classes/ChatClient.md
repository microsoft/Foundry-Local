[**@prathikrao/foundry-local-sdk**](../README.md)

***

[@prathikrao/foundry-local-sdk](../globals.md) / ChatClient

# Class: ChatClient

Defined in: [openai/chatClient.ts:50](https://github.com/microsoft/Foundry-Local/blob/a42a7bf2423d2b1da6cca82531f4977f139aef46/sdk_v2/js/src/openai/chatClient.ts#L50)

Client for performing chat completions with a loaded model.
Follows the OpenAI Chat Completion API structure.

## Constructors

### Constructor

> **new ChatClient**(`modelId`, `coreInterop`): `ChatClient`

Defined in: [openai/chatClient.ts:64](https://github.com/microsoft/Foundry-Local/blob/a42a7bf2423d2b1da6cca82531f4977f139aef46/sdk_v2/js/src/openai/chatClient.ts#L64)

**`Internal`**

Restricted to internal use because CoreInterop is an internal implementation detail.
Users should create clients via the Model.createChatClient() factory method.

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

Defined in: [openai/chatClient.ts:57](https://github.com/microsoft/Foundry-Local/blob/a42a7bf2423d2b1da6cca82531f4977f139aef46/sdk_v2/js/src/openai/chatClient.ts#L57)

Configuration settings for chat completions.

## Methods

### completeChat()

> **completeChat**(`messages`): `Promise`\<`any`\>

Defined in: [openai/chatClient.ts:96](https://github.com/microsoft/Foundry-Local/blob/a42a7bf2423d2b1da6cca82531f4977f139aef46/sdk_v2/js/src/openai/chatClient.ts#L96)

Performs a synchronous chat completion.

#### Parameters

##### messages

`any`[]

An array of message objects (e.g., { role: 'user', content: 'Hello' }).

#### Returns

`Promise`\<`any`\>

The chat completion response object.

#### Throws

Error - If messages are invalid or completion fails.

***

### completeStreamingChat()

> **completeStreamingChat**(`messages`, `callback`): `Promise`\<`void`\>

Defined in: [openai/chatClient.ts:120](https://github.com/microsoft/Foundry-Local/blob/a42a7bf2423d2b1da6cca82531f4977f139aef46/sdk_v2/js/src/openai/chatClient.ts#L120)

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

#### Throws

Error - If messages or callback are invalid, or streaming fails.
