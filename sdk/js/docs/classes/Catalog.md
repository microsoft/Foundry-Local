[foundry-local-sdk](../README.md) / Catalog

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
getCachedModels(): Promise<IModel[]>;
```

Retrieves a list of all locally cached model variants.
This method is asynchronous as it may involve file I/O or querying the underlying core.

#### Returns

`Promise`\<[`IModel`](../README.md#imodel)[]\>

A Promise that resolves to an array of cached IModel objects.

***

### getLatestVersion()

```ts
getLatestVersion(modelOrModelVariant): Promise<IModel>;
```

Get the latest version of a model.
This is used to check if a newer version of a model is available in the catalog for download.

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `modelOrModelVariant` | [`IModel`](../README.md#imodel) | The model to check for the latest version. |

#### Returns

`Promise`\<[`IModel`](../README.md#imodel)\>

The latest version of the model. Will match the input if it is the latest version.

***

### getLoadedModels()

```ts
getLoadedModels(): Promise<IModel[]>;
```

Retrieves a list of all currently loaded model variants.
This operation is asynchronous because checking the loaded status may involve querying
the underlying core or an external service, which can be an I/O bound operation.

#### Returns

`Promise`\<[`IModel`](../README.md#imodel)[]\>

A Promise that resolves to an array of loaded IModel objects.

***

### getModel()

```ts
getModel(alias): Promise<IModel>;
```

Retrieves a model by its alias.
This method is asynchronous as it may ensure the catalog is up-to-date by fetching from a remote service.

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `alias` | `string` | The alias of the model to retrieve. |

#### Returns

`Promise`\<[`IModel`](../README.md#imodel)\>

A Promise that resolves to the IModel object if found, otherwise throws an error.

#### Throws

Error - If alias is null, undefined, or empty.

***

### getModels()

```ts
getModels(): Promise<IModel[]>;
```

Lists all available models in the catalog.
This method is asynchronous as it may fetch the model list from a remote service or perform file I/O.

#### Returns

`Promise`\<[`IModel`](../README.md#imodel)[]\>

A Promise that resolves to an array of IModel objects.

***

### getModelVariant()

```ts
getModelVariant(modelId): Promise<IModel>;
```

Retrieves a specific model variant by its ID.
NOTE: This will return an IModel with a single variant. Use getModel to get an IModel with all available
variants.
This method is asynchronous as it may ensure the catalog is up-to-date by fetching from a remote service.

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `modelId` | `string` | The unique identifier of the model variant. |

#### Returns

`Promise`\<[`IModel`](../README.md#imodel)\>

A Promise that resolves to the IModel object if found, otherwise throws an error.

#### Throws

Error - If modelId is null, undefined, or empty.
