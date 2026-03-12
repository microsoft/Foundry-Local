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

#### Overload 1: Async iterable pattern (recommended)

```ts
completeStreamingChat(messages): AsyncIterable<any>;
```

Performs a streaming chat completion, returning an async iterable. This follows the same pattern as the OpenAI SDK:

```ts
for await (const chunk of chatClient.completeStreamingChat(messages)) {
  process.stdout.write(chunk.choices?.[0]?.delta?.content ?? '');
}
```

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `messages` | `any`[] | An array of message objects. |

#### Returns

`AsyncIterable`\<`any`\>

An async iterable that yields each chunk of the streaming response.

#### Throws

Error - If messages are invalid, or streaming fails.

***

#### Overload 2: Async iterable pattern with tools

```ts
completeStreamingChat(messages, tools): AsyncIterable<any>;
```

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `messages` | `any`[] | An array of message objects. |
| `tools` | `any`[] | An array of tool objects. |

#### Returns

`AsyncIterable`\<`any`\>

An async iterable that yields each chunk of the streaming response.

#### Throws

Error - If messages or tools are invalid, or streaming fails.

***

#### Overload 3: Callback pattern (backward-compatible)

```ts
completeStreamingChat(messages, callback): Promise<void>;
```

Performs a streaming chat completion using a callback for each chunk. This pattern is kept for backward compatibility.

```ts
await chatClient.completeStreamingChat(messages, (chunk) => {
  process.stdout.write(chunk.choices?.[0]?.delta?.content ?? '');
});
```

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

***

#### Overload 4: Callback pattern with tools (backward-compatible)

```ts
completeStreamingChat(messages, tools, callback): Promise<void>;
```

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `messages` | `any`[] | An array of message objects. |
| `tools` | `any`[] | An array of tool objects. |
| `callback` | (`chunk`) => `void` | A callback function that receives each chunk of the streaming response. |

#### Returns

`Promise`\<`void`\>

A promise that resolves when the stream is complete.

#### Throws

Error - If messages, tools, or callback are invalid, or streaming fails.
