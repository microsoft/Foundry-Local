[foundry-local-sdk](../README.md) / ChatClient

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
completeStreamingChat(messages): AsyncIterable<any>;
```

Performs a streaming chat completion, returning an async iterable of chunks.

##### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `messages` | `any`[] | An array of message objects. |

##### Returns

`AsyncIterable`\<`any`\>

An async iterable that yields parsed streaming response chunks.

##### Throws

Error - If messages or tools are invalid, or streaming fails.

##### Example

```typescript
// Without tools:
for await (const chunk of chatClient.completeStreamingChat(messages)) {
    const content = chunk.choices?.[0]?.delta?.content;
    if (content) process.stdout.write(content);
}

// With tools:
for await (const chunk of chatClient.completeStreamingChat(messages, tools)) {
    const content = chunk.choices?.[0]?.delta?.content;
    if (content) process.stdout.write(content);
}
```

#### Call Signature

```ts
completeStreamingChat(messages, tools): AsyncIterable<any>;
```

Performs a streaming chat completion, returning an async iterable of chunks.

##### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `messages` | `any`[] | An array of message objects. |
| `tools` | `any`[] | An optional array of tool objects. |

##### Returns

`AsyncIterable`\<`any`\>

An async iterable that yields parsed streaming response chunks.

##### Throws

Error - If messages or tools are invalid, or streaming fails.

##### Example

```typescript
// Without tools:
for await (const chunk of chatClient.completeStreamingChat(messages)) {
    const content = chunk.choices?.[0]?.delta?.content;
    if (content) process.stdout.write(content);
}

// With tools:
for await (const chunk of chatClient.completeStreamingChat(messages, tools)) {
    const content = chunk.choices?.[0]?.delta?.content;
    if (content) process.stdout.write(content);
}
```
