[@prathikrao/foundry-local-sdk](../README.md) / ModelLoadManager

# Class: ModelLoadManager

Manages the loading and unloading of models.
Handles communication with the core system or an external service (future support).

## Constructors

### Constructor

```ts
new ModelLoadManager(coreInterop, externalServiceUrl?): ModelLoadManager;
```

#### Parameters

| Parameter | Type |
| ------ | ------ |
| `coreInterop` | `CoreInterop` |
| `externalServiceUrl?` | `string` |

#### Returns

`ModelLoadManager`

## Methods

### listLoaded()

```ts
listLoaded(): Promise<string[]>;
```

Lists the IDs of all currently loaded models.

#### Returns

`Promise`\<`string`[]\>

An array of loaded model IDs.

#### Throws

Error - If listing via external service fails or if JSON parsing fails.

***

### load()

```ts
load(modelId): Promise<void>;
```

Loads a model into memory.

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `modelId` | `string` | The ID of the model to load. |

#### Returns

`Promise`\<`void`\>

#### Throws

Error - If loading via external service fails.

***

### unload()

```ts
unload(modelId): Promise<void>;
```

Unloads a model from memory.

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `modelId` | `string` | The ID of the model to unload. |

#### Returns

`Promise`\<`void`\>

#### Throws

Error - If unloading via external service fails.
