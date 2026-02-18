[**@prathikrao/foundry-local-sdk**](../README.md)

***

[@prathikrao/foundry-local-sdk](../globals.md) / Model

# Class: Model

Defined in: [model.ts:10](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/model.ts#L10)

Represents a high-level AI model that may have multiple variants (e.g., quantized versions, different formats).
Manages the selection and interaction with a specific model variant.

## Implements

- [`IModel`](../interfaces/IModel.md)

## Constructors

### Constructor

> **new Model**(`variant`): `Model`

Defined in: [model.ts:16](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/model.ts#L16)

#### Parameters

##### variant

[`ModelVariant`](ModelVariant.md)

#### Returns

`Model`

## Accessors

### alias

#### Get Signature

> **get** **alias**(): `string`

Defined in: [model.ts:67](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/model.ts#L67)

Gets the alias of the model.

##### Returns

`string`

The model alias.

#### Implementation of

[`IModel`](../interfaces/IModel.md).[`alias`](../interfaces/IModel.md#alias)

***

### id

#### Get Signature

> **get** **id**(): `string`

Defined in: [model.ts:59](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/model.ts#L59)

Gets the ID of the currently selected variant.

##### Returns

`string`

The ID of the selected variant.

#### Implementation of

[`IModel`](../interfaces/IModel.md).[`id`](../interfaces/IModel.md#id)

***

### isCached

#### Get Signature

> **get** **isCached**(): `boolean`

Defined in: [model.ts:75](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/model.ts#L75)

Checks if the currently selected variant is cached locally.

##### Returns

`boolean`

True if cached, false otherwise.

#### Implementation of

[`IModel`](../interfaces/IModel.md).[`isCached`](../interfaces/IModel.md#iscached)

***

### path

#### Get Signature

> **get** **path**(): `string`

Defined in: [model.ts:107](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/model.ts#L107)

Gets the local file path of the currently selected variant.

##### Returns

`string`

The local file path.

#### Implementation of

[`IModel`](../interfaces/IModel.md).[`path`](../interfaces/IModel.md#path)

***

### variants

#### Get Signature

> **get** **variants**(): [`ModelVariant`](ModelVariant.md)[]

Defined in: [model.ts:91](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/model.ts#L91)

Gets all available variants for this model.

##### Returns

[`ModelVariant`](ModelVariant.md)[]

An array of ModelVariant objects.

## Methods

### addVariant()

> **addVariant**(`variant`): `void`

Defined in: [model.ts:28](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/model.ts#L28)

Adds a new variant to this model.
Automatically selects the new variant if it is cached and the current one is not.

#### Parameters

##### variant

[`ModelVariant`](ModelVariant.md)

The model variant to add.

#### Returns

`void`

#### Throws

Error - If the variant's alias does not match the model's alias.

***

### createAudioClient()

> **createAudioClient**(): [`AudioClient`](AudioClient.md)

Defined in: [model.ts:146](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/model.ts#L146)

Creates an AudioClient for interacting with the model via audio operations.

#### Returns

[`AudioClient`](AudioClient.md)

An AudioClient instance.

#### Implementation of

[`IModel`](../interfaces/IModel.md).[`createAudioClient`](../interfaces/IModel.md#createaudioclient)

***

### createChatClient()

> **createChatClient**(): [`ChatClient`](ChatClient.md)

Defined in: [model.ts:138](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/model.ts#L138)

Creates a ChatClient for interacting with the model via chat completions.

#### Returns

[`ChatClient`](ChatClient.md)

A ChatClient instance.

#### Implementation of

[`IModel`](../interfaces/IModel.md).[`createChatClient`](../interfaces/IModel.md#createchatclient)

***

### download()

> **download**(`progressCallback?`): `void`

Defined in: [model.ts:99](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/model.ts#L99)

Downloads the currently selected variant.

#### Parameters

##### progressCallback?

(`progress`) => `void`

Optional callback to report download progress.

#### Returns

`void`

#### Implementation of

[`IModel`](../interfaces/IModel.md).[`download`](../interfaces/IModel.md#download)

***

### isLoaded()

> **isLoaded**(): `Promise`\<`boolean`\>

Defined in: [model.ts:83](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/model.ts#L83)

Checks if the currently selected variant is loaded in memory.

#### Returns

`Promise`\<`boolean`\>

True if loaded, false otherwise.

#### Implementation of

[`IModel`](../interfaces/IModel.md).[`isLoaded`](../interfaces/IModel.md#isloaded)

***

### load()

> **load**(): `Promise`\<`void`\>

Defined in: [model.ts:115](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/model.ts#L115)

Loads the currently selected variant into memory.

#### Returns

`Promise`\<`void`\>

A promise that resolves when the model is loaded.

#### Implementation of

[`IModel`](../interfaces/IModel.md).[`load`](../interfaces/IModel.md#load)

***

### removeFromCache()

> **removeFromCache**(): `void`

Defined in: [model.ts:122](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/model.ts#L122)

Removes the currently selected variant from the local cache.

#### Returns

`void`

#### Implementation of

[`IModel`](../interfaces/IModel.md).[`removeFromCache`](../interfaces/IModel.md#removefromcache)

***

### selectVariant()

> **selectVariant**(`modelId`): `void`

Defined in: [model.ts:45](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/model.ts#L45)

Selects a specific variant by its ID.

#### Parameters

##### modelId

`string`

The ID of the variant to select.

#### Returns

`void`

#### Throws

Error - If the variant with the specified ID is not found.

***

### unload()

> **unload**(): `Promise`\<`void`\>

Defined in: [model.ts:130](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/model.ts#L130)

Unloads the currently selected variant from memory.

#### Returns

`Promise`\<`void`\>

A promise that resolves when the model is unloaded.

#### Implementation of

[`IModel`](../interfaces/IModel.md).[`unload`](../interfaces/IModel.md#unload)
