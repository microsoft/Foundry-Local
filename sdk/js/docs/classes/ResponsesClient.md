[@prathikrao/foundry-local-sdk](../README.md) / ResponsesClient

# Class: ResponsesClient

Client for the OpenAI Responses API served by Foundry Local's embedded web service.

Unlike ChatClient/AudioClient (which use FFI via CoreInterop), the Responses API
is HTTP-only. This client uses fetch() for all operations and parses Server-Sent Events
for streaming.

Create via `FoundryLocalManager.createResponsesClient()` or
`model.createResponsesClient(baseUrl)`.

## Example

```typescript
const manager = FoundryLocalManager.create({ appName: 'MyApp' });
manager.startWebService();
const client = manager.createResponsesClient('my-model-id');

// Non-streaming
const response = await client.create('Hello, world!');
console.log(response.output);

// Streaming
await client.createStreaming('Tell me a story', (event) => {
    if (event.type === 'response.output_text.delta') {
        process.stdout.write(event.delta);
    }
});
```

## Constructors

### Constructor

```ts
new ResponsesClient(baseUrl, modelId?): ResponsesClient;
```

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `baseUrl` | `string` | The base URL of the Foundry Local web service (e.g. "http://127.0.0.1:5273"). |
| `modelId?` | `string` | Optional default model ID. Can be overridden per-request via options. |

#### Returns

`ResponsesClient`

## Properties

### settings

```ts
settings: ResponsesClientSettings;
```

Configuration settings for responses.

## Methods

### cancel()

```ts
cancel(responseId): Promise<ResponseObject>;
```

Cancels an in-progress response.

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `responseId` | `string` | The ID of the response to cancel. |

#### Returns

`Promise`\<[`ResponseObject`](../README.md#responseobject)\>

The cancelled Response object.

***

### create()

```ts
create(input, options?): Promise<ResponseObject>;
```

Creates a model response (non-streaming).

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `input` | `string` \| [`ResponseInputItem`](../README.md#responseinputitem)[] | A string prompt or array of input items. |
| `options?` | `Partial`\<[`ResponseCreateParams`](../README.md#responsecreateparams)\> | Additional request parameters that override client settings. The `model` field is optional here if a default model was set in the constructor. |

#### Returns

`Promise`\<[`ResponseObject`](../README.md#responseobject)\>

The completed Response object. Check `response.status` and `response.error`
  even on success — the server returns HTTP 200 for model-level failures too.

***

### createStreaming()

```ts
createStreaming(
   input, 
   callback, 
options?): Promise<void>;
```

Creates a model response with streaming via Server-Sent Events.

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `input` | `string` \| [`ResponseInputItem`](../README.md#responseinputitem)[] | A string prompt or array of input items. |
| `callback` | (`event`) => `void` | Called for each streaming event received. |
| `options?` | `Partial`\<[`ResponseCreateParams`](../README.md#responsecreateparams)\> | Additional request parameters that override client settings. |

#### Returns

`Promise`\<`void`\>

***

### delete()

```ts
delete(responseId): Promise<DeleteResponseResult>;
```

Deletes a stored response by ID.

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `responseId` | `string` | The ID of the response to delete. |

#### Returns

`Promise`\<[`DeleteResponseResult`](../README.md#deleteresponseresult)\>

The deletion result.

***

### get()

```ts
get(responseId): Promise<ResponseObject>;
```

Retrieves a stored response by ID.

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `responseId` | `string` | The ID of the response to retrieve. |

#### Returns

`Promise`\<[`ResponseObject`](../README.md#responseobject)\>

The Response object, or throws if not found.

***

### getInputItems()

```ts
getInputItems(responseId): Promise<InputItemsListResponse>;
```

Retrieves input items for a stored response.

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `responseId` | `string` | The ID of the response. |

#### Returns

`Promise`\<[`InputItemsListResponse`](../README.md#inputitemslistresponse)\>

The list of input items.
