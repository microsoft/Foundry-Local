[**@prathikrao/foundry-local-sdk**](../README.md)

***

[@prathikrao/foundry-local-sdk](../globals.md) / ModelLoadManager

# Class: ModelLoadManager

Defined in: [detail/modelLoadManager.ts:9](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/detail/modelLoadManager.ts#L9)

Manages the loading and unloading of models.
Handles communication with the core system or an external service (future support).

## Constructors

### Constructor

> **new ModelLoadManager**(`coreInterop`, `externalServiceUrl?`): `ModelLoadManager`

Defined in: [detail/modelLoadManager.ts:14](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/detail/modelLoadManager.ts#L14)

#### Parameters

##### coreInterop

[`CoreInterop`](CoreInterop.md)

##### externalServiceUrl?

`string`

#### Returns

`ModelLoadManager`

## Methods

### listLoaded()

> **listLoaded**(): `Promise`\<`string`[]\>

Defined in: [detail/modelLoadManager.ts:65](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/detail/modelLoadManager.ts#L65)

Lists the IDs of all currently loaded models.

#### Returns

`Promise`\<`string`[]\>

An array of loaded model IDs.

#### Throws

Error - If listing via external service fails or if JSON parsing fails.

***

### load()

> **load**(`modelId`): `Promise`\<`void`\>

Defined in: [detail/modelLoadManager.ts:27](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/detail/modelLoadManager.ts#L27)

Loads a model into memory.

#### Parameters

##### modelId

`string`

The ID of the model to load.

#### Returns

`Promise`\<`void`\>

#### Throws

Error - If loading via external service fails.

***

### unload()

> **unload**(`modelId`): `Promise`\<`void`\>

Defined in: [detail/modelLoadManager.ts:48](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/detail/modelLoadManager.ts#L48)

Unloads a model from memory.

#### Parameters

##### modelId

`string`

The ID of the model to unload.

#### Returns

`Promise`\<`void`\>

#### Throws

Error - If unloading via external service fails.
