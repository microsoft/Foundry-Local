[@prathikrao/foundry-local-sdk](../README.md) / Model

# Class: Model

Represents a high-level AI model that may have multiple variants (e.g., quantized versions, different formats).
Manages the selection and interaction with a specific model variant.

## Implements

- [`IModel`](../README.md#imodel)

## Constructors

### Constructor

```ts
new Model(variant): Model;
```

#### Parameters

| Parameter | Type |
| ------ | ------ |
| `variant` | [`ModelVariant`](ModelVariant.md) |

#### Returns

`Model`

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

### id

#### Get Signature

```ts
get id(): string;
```

Gets the ID of the currently selected variant.

##### Returns

`string`

The ID of the selected variant.

#### Implementation of

[`IModel`](../README.md#imodel).[`id`](../README.md#id)

***

### isCached

#### Get Signature

```ts
get isCached(): boolean;
```

Checks if the currently selected variant is cached locally.

##### Returns

`boolean`

True if cached, false otherwise.

#### Implementation of

[`IModel`](../README.md#imodel).[`isCached`](../README.md#iscached)

***

### path

#### Get Signature

```ts
get path(): string;
```

Gets the local file path of the currently selected variant.

##### Returns

`string`

The local file path.

#### Implementation of

[`IModel`](../README.md#imodel).[`path`](../README.md#path)

***

### variants

#### Get Signature

```ts
get variants(): ModelVariant[];
```

Gets all available variants for this model.

##### Returns

[`ModelVariant`](ModelVariant.md)[]

An array of ModelVariant objects.

## Methods

### addVariant()

```ts
addVariant(variant): void;
```

Adds a new variant to this model.
Automatically selects the new variant if it is cached and the current one is not.

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `variant` | [`ModelVariant`](ModelVariant.md) | The model variant to add. |

#### Returns

`void`

#### Throws

Error - If the variant's alias does not match the model's alias.

***

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

### download()

```ts
download(progressCallback?): void;
```

Downloads the currently selected variant.

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `progressCallback?` | (`progress`) => `void` | Optional callback to report download progress. |

#### Returns

`void`

#### Implementation of

[`IModel`](../README.md#imodel).[`download`](../README.md#download)

***

### isLoaded()

```ts
isLoaded(): Promise<boolean>;
```

Checks if the currently selected variant is loaded in memory.

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

Loads the currently selected variant into memory.

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

Removes the currently selected variant from the local cache.

#### Returns

`void`

#### Implementation of

[`IModel`](../README.md#imodel).[`removeFromCache`](../README.md#removefromcache)

***

### selectVariant()

```ts
selectVariant(modelId): void;
```

Selects a specific variant by its ID.

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `modelId` | `string` | The ID of the variant to select. |

#### Returns

`void`

#### Throws

Error - If the variant with the specified ID is not found.

***

### unload()

```ts
unload(): Promise<void>;
```

Unloads the currently selected variant from memory.

#### Returns

`Promise`\<`void`\>

A promise that resolves when the model is unloaded.

#### Implementation of

[`IModel`](../README.md#imodel).[`unload`](../README.md#unload)
