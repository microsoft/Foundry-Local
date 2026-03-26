[foundry-local-sdk](../README.md) / ModelVariant

# Class: ModelVariant

Represents a specific variant of a model (e.g., a specific quantization or format).
Contains the low-level implementation for interacting with the model.

## Implements

- [`IModel`](../README.md#imodel)

## Constructors

### Constructor

```ts
new ModelVariant(
   modelInfo, 
   coreInterop, 
   modelLoadManager): ModelVariant;
```

#### Parameters

| Parameter | Type |
| ------ | ------ |
| `modelInfo` | [`ModelInfo`](../README.md#modelinfo) |
| `coreInterop` | `CoreInterop` |
| `modelLoadManager` | [`ModelLoadManager`](ModelLoadManager.md) |

#### Returns

`ModelVariant`

## Accessors

### alias

#### Get Signature

```ts
get alias(): string;
```

Gets the alias of the model.

##### Returns

`string`

The model alias.

#### Implementation of

[`IModel`](../README.md#imodel).[`alias`](../README.md#alias)

***

### capabilities

#### Get Signature

```ts
get capabilities(): string | null | undefined;
```

##### Returns

`string` \| `null` \| `undefined`

#### Implementation of

[`IModel`](../README.md#imodel).[`capabilities`](../README.md#capabilities)

***

### contextLength

#### Get Signature

```ts
get contextLength(): number | null | undefined;
```

##### Returns

`number` \| `null` \| `undefined`

#### Implementation of

[`IModel`](../README.md#imodel).[`contextLength`](../README.md#contextlength)

***

### id

#### Get Signature

```ts
get id(): string;
```

Gets the unique identifier of the model variant.

##### Returns

`string`

The model ID.

#### Implementation of

[`IModel`](../README.md#imodel).[`id`](../README.md#id-3)

***

### inputModalities

#### Get Signature

```ts
get inputModalities(): string | null | undefined;
```

##### Returns

`string` \| `null` \| `undefined`

#### Implementation of

[`IModel`](../README.md#imodel).[`inputModalities`](../README.md#inputmodalities)

***

### isCached

#### Get Signature

```ts
get isCached(): boolean;
```

Checks if the model variant is cached locally.

##### Returns

`boolean`

True if cached, false otherwise.

#### Implementation of

[`IModel`](../README.md#imodel).[`isCached`](../README.md#iscached)

***

### modelInfo

#### Get Signature

```ts
get modelInfo(): ModelInfo;
```

Gets the detailed information about the model variant.

##### Returns

[`ModelInfo`](../README.md#modelinfo)

The ModelInfo object.

***

### outputModalities

#### Get Signature

```ts
get outputModalities(): string | null | undefined;
```

##### Returns

`string` \| `null` \| `undefined`

#### Implementation of

[`IModel`](../README.md#imodel).[`outputModalities`](../README.md#outputmodalities)

***

### path

#### Get Signature

```ts
get path(): string;
```

Gets the local file path of the model variant.

##### Returns

`string`

The local file path.

#### Implementation of

[`IModel`](../README.md#imodel).[`path`](../README.md#path)

***

### supportsToolCalling

#### Get Signature

```ts
get supportsToolCalling(): boolean | null | undefined;
```

##### Returns

`boolean` \| `null` \| `undefined`

#### Implementation of

[`IModel`](../README.md#imodel).[`supportsToolCalling`](../README.md#supportstoolcalling)

## Methods

### createAudioClient()

```ts
createAudioClient(): AudioClient;
```

Creates an AudioClient for interacting with the model via audio operations.

#### Returns

[`AudioClient`](AudioClient.md)

An AudioClient instance.

#### Implementation of

[`IModel`](../README.md#imodel).[`createAudioClient`](../README.md#createaudioclient)

***

### createChatClient()

```ts
createChatClient(): ChatClient;
```

Creates a ChatClient for interacting with the model via chat completions.

#### Returns

[`ChatClient`](ChatClient.md)

A ChatClient instance.

#### Implementation of

[`IModel`](../README.md#imodel).[`createChatClient`](../README.md#createchatclient)

***

### createResponsesClient()

```ts
createResponsesClient(baseUrl): ResponsesClient;
```

Creates a ResponsesClient for interacting with the model via the Responses API.

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `baseUrl` | `string` | The base URL of the Foundry Local web service. |

#### Returns

[`ResponsesClient`](ResponsesClient.md)

A ResponsesClient instance.

#### Implementation of

[`IModel`](../README.md#imodel).[`createResponsesClient`](../README.md#createresponsesclient)

***

### download()

```ts
download(progressCallback?): Promise<void>;
```

Downloads the model variant.

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `progressCallback?` | (`progress`) => `void` | Optional callback to report download progress (0-100). |

#### Returns

`Promise`\<`void`\>

#### Implementation of

[`IModel`](../README.md#imodel).[`download`](../README.md#download)

***

### isLoaded()

```ts
isLoaded(): Promise<boolean>;
```

Checks if the model variant is loaded in memory.

#### Returns

`Promise`\<`boolean`\>

True if loaded, false otherwise.

#### Implementation of

[`IModel`](../README.md#imodel).[`isLoaded`](../README.md#isloaded)

***

### load()

```ts
load(): Promise<void>;
```

Loads the model variant into memory.

#### Returns

`Promise`\<`void`\>

A promise that resolves when the model is loaded.

#### Implementation of

[`IModel`](../README.md#imodel).[`load`](../README.md#load)

***

### removeFromCache()

```ts
removeFromCache(): void;
```

Removes the model variant from the local cache.

#### Returns

`void`

#### Implementation of

[`IModel`](../README.md#imodel).[`removeFromCache`](../README.md#removefromcache)

***

### unload()

```ts
unload(): Promise<void>;
```

Unloads the model variant from memory.

#### Returns

`Promise`\<`void`\>

A promise that resolves when the model is unloaded.

#### Implementation of

[`IModel`](../README.md#imodel).[`unload`](../README.md#unload)
