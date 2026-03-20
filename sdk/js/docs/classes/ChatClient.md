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

#### Call Signature

```ts
completeChat(messages): Promise<any>;
```

Performs a synchronous chat completion.

##### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `messages` | `any`[] | An array of message objects (e.g., { role: 'user', content: 'Hello' }). |

##### Returns

`Promise`\<`any`\>

The chat completion response object.

##### Throws

Error - If messages or tools are invalid or completion fails.

#### Call Signature

```ts
completeChat(messages, tools): Promise<any>;
```

Performs a synchronous chat completion.

##### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `messages` | `any`[] | An array of message objects (e.g., { role: 'user', content: 'Hello' }). |
| `tools` | `any`[] | An array of tool objects (e.g. { type: 'function', function: { name: 'get_apps', description: 'Returns a list of apps available on the system' } }). |

##### Returns

`Promise`\<`any`\>

The chat completion response object.

##### Throws

Error - If messages or tools are invalid or completion fails.

***

### completeStreamingChat()

#### Call Signature

```ts
completeStreamingChat(messages, callback): Promise<void>;
```

Performs a streaming chat completion.

##### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `messages` | `any`[] | An array of message objects. |
| `callback` | (`chunk`) => `void` | A callback function that receives each chunk of the streaming response. |

##### Returns

`Promise`\<`void`\>

A promise that resolves when the stream is complete.

##### Throws

Error - If messages, tools, or callback are invalid, or streaming fails.

#### Call Signature

```ts
completeStreamingChat(
   messages, 
   tools, 
callback): Promise<void>;
```

Performs a streaming chat completion.

##### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `messages` | `any`[] | An array of message objects. |
| `tools` | `any`[] | An array of tool objects. |
| `callback` | (`chunk`) => `void` | A callback function that receives each chunk of the streaming response. |

##### Returns

`Promise`\<`void`\>

A promise that resolves when the stream is complete.

##### Throws

Error - If messages, tools, or callback are invalid, or streaming fails.
