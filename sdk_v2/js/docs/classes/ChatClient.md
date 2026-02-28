[@prathikrao/foundry-local-sdk](../README.md) / ChatClient

# Class: ChatClient

Client for performing chat completions with a loaded model.
Follows the OpenAI Chat Completion API structure.

## Properties

### settings

```ts
settings: ChatClientSettings;
```

Configuration settings for chat completions.

## Methods

### completeChat()

```ts
completeChat(messages): Promise<any>;
```

Performs a synchronous chat completion.

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `messages` | `any`[] | An array of message objects (e.g., { role: 'user', content: 'Hello' }). |

#### Returns

`Promise`\<`any`\>

The chat completion response object.

#### Throws

Error - If messages are invalid or completion fails.

***

### completeStreamingChat()

```ts
completeStreamingChat(messages, callback): Promise<void>;
```

Performs a streaming chat completion.

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `messages` | `any`[] | An array of message objects. |
| `callback` | (`chunk`) => `void` | A callback function that receives each chunk of the streaming response. |

#### Returns

`Promise`\<`void`\>

A promise that resolves when the stream is complete.

#### Throws

Error - If messages or callback are invalid, or streaming fails.
