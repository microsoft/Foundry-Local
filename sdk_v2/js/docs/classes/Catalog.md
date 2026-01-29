[**@prathikrao/foundry-local-sdk**](../README.md)

***

[@prathikrao/foundry-local-sdk](../globals.md) / Catalog

# Class: Catalog

Defined in: [catalog.ts:11](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/catalog.ts#L11)

Represents a catalog of AI models available in the system.
Provides methods to discover, list, and retrieve models and their variants.

## Constructors

### Constructor

> **new Catalog**(`coreInterop`, `modelLoadManager`): `Catalog`

Defined in: [catalog.ts:20](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/catalog.ts#L20)

#### Parameters

##### coreInterop

[`CoreInterop`](CoreInterop.md)

##### modelLoadManager

[`ModelLoadManager`](ModelLoadManager.md)

#### Returns

`Catalog`

## Accessors

### name

#### Get Signature

> **get** **name**(): `string`

Defined in: [catalog.ts:30](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/catalog.ts#L30)

Gets the name of the catalog.

##### Returns

`string`

The name of the catalog.

## Methods

### getCachedModels()

> **getCachedModels**(): `Promise`\<[`ModelVariant`](ModelVariant.md)[]\>

Defined in: [catalog.ts:108](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/catalog.ts#L108)

Retrieves a list of all locally cached model variants.
This method is asynchronous as it may involve file I/O or querying the underlying core.

#### Returns

`Promise`\<[`ModelVariant`](ModelVariant.md)[]\>

A Promise that resolves to an array of cached ModelVariant objects.

***

### getLoadedModels()

> **getLoadedModels**(): `Promise`\<[`ModelVariant`](ModelVariant.md)[]\>

Defined in: [catalog.ts:134](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/catalog.ts#L134)

Retrieves a list of all currently loaded model variants.
This operation is asynchronous because checking the loaded status may involve querying
the underlying core or an external service, which can be an I/O bound operation.

#### Returns

`Promise`\<[`ModelVariant`](ModelVariant.md)[]\>

A Promise that resolves to an array of loaded ModelVariant objects.

***

### getModel()

> **getModel**(`alias`): `Promise`\<[`Model`](Model.md) \| `undefined`\>

Defined in: [catalog.ts:87](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/catalog.ts#L87)

Retrieves a model by its alias.
This method is asynchronous as it may ensure the catalog is up-to-date by fetching from a remote service.

#### Parameters

##### alias

`string`

The alias of the model to retrieve.

#### Returns

`Promise`\<[`Model`](Model.md) \| `undefined`\>

A Promise that resolves to the Model object if found, otherwise undefined.

***

### getModels()

> **getModels**(): `Promise`\<[`Model`](Model.md)[]\>

Defined in: [catalog.ts:76](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/catalog.ts#L76)

Lists all available models in the catalog.
This method is asynchronous as it may fetch the model list from a remote service or perform file I/O.

#### Returns

`Promise`\<[`Model`](Model.md)[]\>

A Promise that resolves to an array of Model objects.

***

### getModelVariant()

> **getModelVariant**(`modelId`): `Promise`\<[`ModelVariant`](ModelVariant.md) \| `undefined`\>

Defined in: [catalog.ts:98](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/catalog.ts#L98)

Retrieves a specific model variant by its ID.
This method is asynchronous as it may ensure the catalog is up-to-date by fetching from a remote service.

#### Parameters

##### modelId

`string`

The unique identifier of the model variant.

#### Returns

`Promise`\<[`ModelVariant`](ModelVariant.md) \| `undefined`\>

A Promise that resolves to the ModelVariant object if found, otherwise undefined.
