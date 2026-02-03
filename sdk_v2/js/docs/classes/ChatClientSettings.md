[**@prathikrao/foundry-local-sdk**](../README.md)

***

[@prathikrao/foundry-local-sdk](../globals.md) / ChatClientSettings

# Class: ChatClientSettings

Defined in: [openai/chatClient.ts:3](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/openai/chatClient.ts#L3)

## Constructors

### Constructor

> **new ChatClientSettings**(): `ChatClientSettings`

#### Returns

`ChatClientSettings`

## Properties

### frequencyPenalty?

> `optional` **frequencyPenalty**: `number`

Defined in: [openai/chatClient.ts:4](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/openai/chatClient.ts#L4)

***

### maxTokens?

> `optional` **maxTokens**: `number`

Defined in: [openai/chatClient.ts:5](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/openai/chatClient.ts#L5)

***

### n?

> `optional` **n**: `number`

Defined in: [openai/chatClient.ts:6](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/openai/chatClient.ts#L6)

***

### presencePenalty?

> `optional` **presencePenalty**: `number`

Defined in: [openai/chatClient.ts:8](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/openai/chatClient.ts#L8)

***

### randomSeed?

> `optional` **randomSeed**: `number`

Defined in: [openai/chatClient.ts:9](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/openai/chatClient.ts#L9)

***

### temperature?

> `optional` **temperature**: `number`

Defined in: [openai/chatClient.ts:7](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/openai/chatClient.ts#L7)

***

### topK?

> `optional` **topK**: `number`

Defined in: [openai/chatClient.ts:10](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/openai/chatClient.ts#L10)

***

### topP?

> `optional` **topP**: `number`

Defined in: [openai/chatClient.ts:11](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/openai/chatClient.ts#L11)

## Methods

### \_serialize()

> **\_serialize**(): `object`

Defined in: [openai/chatClient.ts:17](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/openai/chatClient.ts#L17)

**`Internal`**

Serializes the settings into an OpenAI-compatible request object.

#### Returns

`object`
