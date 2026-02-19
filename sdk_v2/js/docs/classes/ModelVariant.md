[**@prathikrao/foundry-local-sdk**](../README.md)

***

[@prathikrao/foundry-local-sdk](../globals.md) / ModelVariant

# Class: ModelVariant

Defined in: [modelVariant.ts:12](https://github.com/microsoft/Foundry-Local/blob/69c510db89a256a06600e9feab0639030838b463/sdk_v2/js/src/modelVariant.ts#L12)

Represents a specific variant of a model (e.g., a specific quantization or format).
Contains the low-level implementation for interacting with the model.

## Implements

- [`IModel`](../interfaces/IModel.md)

## Constructors

### Constructor

> **new ModelVariant**(`modelInfo`, `coreInterop`, `modelLoadManager`): `ModelVariant`

Defined in: [modelVariant.ts:17](https://github.com/microsoft/Foundry-Local/blob/69c510db89a256a06600e9feab0639030838b463/sdk_v2/js/src/modelVariant.ts#L17)

#### Parameters

##### modelInfo

[`ModelInfo`](../interfaces/ModelInfo.md)

##### coreInterop

[`CoreInterop`](CoreInterop.md)

##### modelLoadManager

[`ModelLoadManager`](ModelLoadManager.md)

#### Returns

`ModelVariant`

## Accessors

### alias

#### Get Signature

> **get** **alias**(): `string`

Defined in: [modelVariant.ts:35](https://github.com/microsoft/Foundry-Local/blob/69c510db89a256a06600e9feab0639030838b463/sdk_v2/js/src/modelVariant.ts#L35)

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

Defined in: [modelVariant.ts:27](https://github.com/microsoft/Foundry-Local/blob/69c510db89a256a06600e9feab0639030838b463/sdk_v2/js/src/modelVariant.ts#L27)

Gets the unique identifier of the model variant.

##### Returns

`string`

The model ID.

#### Implementation of

[`IModel`](../interfaces/IModel.md).[`id`](../interfaces/IModel.md#id)

***

### isCached

#### Get Signature

> **get** **isCached**(): `boolean`

Defined in: [modelVariant.ts:51](https://github.com/microsoft/Foundry-Local/blob/69c510db89a256a06600e9feab0639030838b463/sdk_v2/js/src/modelVariant.ts#L51)

Checks if the model variant is cached locally.

##### Returns

`boolean`

True if cached, false otherwise.

#### Implementation of

[`IModel`](../interfaces/IModel.md).[`isCached`](../interfaces/IModel.md#iscached)

***

### modelInfo

#### Get Signature

> **get** **modelInfo**(): [`ModelInfo`](../interfaces/ModelInfo.md)

Defined in: [modelVariant.ts:43](https://github.com/microsoft/Foundry-Local/blob/69c510db89a256a06600e9feab0639030838b463/sdk_v2/js/src/modelVariant.ts#L43)

Gets the detailed information about the model variant.

##### Returns

[`ModelInfo`](../interfaces/ModelInfo.md)

The ModelInfo object.

***

### path

#### Get Signature

> **get** **path**(): `string`

Defined in: [modelVariant.ts:83](https://github.com/microsoft/Foundry-Local/blob/69c510db89a256a06600e9feab0639030838b463/sdk_v2/js/src/modelVariant.ts#L83)

Gets the local file path of the model variant.

##### Returns

`string`

The local file path.

#### Implementation of

[`IModel`](../interfaces/IModel.md).[`path`](../interfaces/IModel.md#path)

## Methods

### createAudioClient()

> **createAudioClient**(): [`AudioClient`](AudioClient.md)

Defined in: [modelVariant.ts:123](https://github.com/microsoft/Foundry-Local/blob/69c510db89a256a06600e9feab0639030838b463/sdk_v2/js/src/modelVariant.ts#L123)

Creates an AudioClient for interacting with the model via audio operations.

#### Returns

[`AudioClient`](AudioClient.md)

An AudioClient instance.

#### Implementation of

[`IModel`](../interfaces/IModel.md).[`createAudioClient`](../interfaces/IModel.md#createaudioclient)

***

### createChatClient()

> **createChatClient**(): [`ChatClient`](ChatClient.md)

Defined in: [modelVariant.ts:115](https://github.com/microsoft/Foundry-Local/blob/69c510db89a256a06600e9feab0639030838b463/sdk_v2/js/src/modelVariant.ts#L115)

Creates a ChatClient for interacting with the model via chat completions.

#### Returns

[`ChatClient`](ChatClient.md)

A ChatClient instance.

#### Implementation of

[`IModel`](../interfaces/IModel.md).[`createChatClient`](../interfaces/IModel.md#createchatclient)

***

### download()

> **download**(`progressCallback?`): `void`

Defined in: [modelVariant.ts:70](https://github.com/microsoft/Foundry-Local/blob/69c510db89a256a06600e9feab0639030838b463/sdk_v2/js/src/modelVariant.ts#L70)

Downloads the model variant.

#### Parameters

##### progressCallback?

(`progress`) => `void`

Optional callback to report download progress.

#### Returns

`void`

#### Throws

Error - If progress callback is provided (not implemented).

#### Implementation of

[`IModel`](../interfaces/IModel.md).[`download`](../interfaces/IModel.md#download)

***

### isLoaded()

> **isLoaded**(): `Promise`\<`boolean`\>

Defined in: [modelVariant.ts:60](https://github.com/microsoft/Foundry-Local/blob/69c510db89a256a06600e9feab0639030838b463/sdk_v2/js/src/modelVariant.ts#L60)

Checks if the model variant is loaded in memory.

#### Returns

`Promise`\<`boolean`\>

True if loaded, false otherwise.

#### Implementation of

[`IModel`](../interfaces/IModel.md).[`isLoaded`](../interfaces/IModel.md#isloaded)

***

### load()

> **load**(): `Promise`\<`void`\>

Defined in: [modelVariant.ts:92](https://github.com/microsoft/Foundry-Local/blob/69c510db89a256a06600e9feab0639030838b463/sdk_v2/js/src/modelVariant.ts#L92)

Loads the model variant into memory.

#### Returns

`Promise`\<`void`\>

A promise that resolves when the model is loaded.

#### Implementation of

[`IModel`](../interfaces/IModel.md).[`load`](../interfaces/IModel.md#load)

***

### removeFromCache()

> **removeFromCache**(): `void`

Defined in: [modelVariant.ts:99](https://github.com/microsoft/Foundry-Local/blob/69c510db89a256a06600e9feab0639030838b463/sdk_v2/js/src/modelVariant.ts#L99)

Removes the model variant from the local cache.

#### Returns

`void`

#### Implementation of

[`IModel`](../interfaces/IModel.md).[`removeFromCache`](../interfaces/IModel.md#removefromcache)

***

### unload()

> **unload**(): `Promise`\<`void`\>

Defined in: [modelVariant.ts:107](https://github.com/microsoft/Foundry-Local/blob/69c510db89a256a06600e9feab0639030838b463/sdk_v2/js/src/modelVariant.ts#L107)

Unloads the model variant from memory.

#### Returns

`Promise`\<`void`\>

A promise that resolves when the model is unloaded.

#### Implementation of

[`IModel`](../interfaces/IModel.md).[`unload`](../interfaces/IModel.md#unload)
