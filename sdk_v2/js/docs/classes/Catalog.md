[@prathikrao/foundry-local-sdk](../README.md) / Catalog

# Class: Catalog

Represents a catalog of AI models available in the system.
Provides methods to discover, list, and retrieve models and their variants.

## Constructors

### Constructor

```ts
new Catalog(coreInterop, modelLoadManager): Catalog;
```

#### Parameters

| Parameter | Type |
| ------ | ------ |
| `coreInterop` | `CoreInterop` |
| `modelLoadManager` | [`ModelLoadManager`](ModelLoadManager.md) |

#### Returns

`Catalog`

## Accessors

### name

#### Get Signature

```ts
get name(): string;
```

Gets the name of the catalog.

##### Returns

`string`

The name of the catalog.

## Methods

### getCachedModels()

```ts
getCachedModels(): Promise<ModelVariant[]>;
```

Retrieves a list of all locally cached model variants.
This method is asynchronous as it may involve file I/O or querying the underlying core.

#### Returns

`Promise`\<[`ModelVariant`](ModelVariant.md)[]\>

A Promise that resolves to an array of cached ModelVariant objects.

***

### getLoadedModels()

```ts
getLoadedModels(): Promise<ModelVariant[]>;
```

Retrieves a list of all currently loaded model variants.
This operation is asynchronous because checking the loaded status may involve querying
the underlying core or an external service, which can be an I/O bound operation.

#### Returns

`Promise`\<[`ModelVariant`](ModelVariant.md)[]\>

A Promise that resolves to an array of loaded ModelVariant objects.

***

### getModel()

```ts
getModel(alias): Promise<Model>;
```

Retrieves a model by its alias.
This method is asynchronous as it may ensure the catalog is up-to-date by fetching from a remote service.

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `alias` | `string` | The alias of the model to retrieve. |

#### Returns

`Promise`\<[`Model`](Model.md)\>

A Promise that resolves to the Model object if found, otherwise throws an error.

#### Throws

Error - If alias is null, undefined, or empty.

***

### getModels()

```ts
getModels(): Promise<Model[]>;
```

Lists all available models in the catalog.
This method is asynchronous as it may fetch the model list from a remote service or perform file I/O.

#### Returns

`Promise`\<[`Model`](Model.md)[]\>

A Promise that resolves to an array of Model objects.

***

### getModelVariant()

```ts
getModelVariant(modelId): Promise<ModelVariant>;
```

Retrieves a specific model variant by its ID.
This method is asynchronous as it may ensure the catalog is up-to-date by fetching from a remote service.

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `modelId` | `string` | The unique identifier of the model variant. |

#### Returns

`Promise`\<[`ModelVariant`](ModelVariant.md)\>

A Promise that resolves to the ModelVariant object if found, otherwise throws an error.

#### Throws

Error - If modelId is null, undefined, or empty.
