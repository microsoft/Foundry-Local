[foundry-local-sdk](../README.md) / FoundryLocalManager

# Class: FoundryLocalManager

The main entry point for the Foundry Local SDK.
Manages the initialization of the core system and provides access to the Catalog and ModelLoadManager.

## Accessors

### catalog

#### Get Signature

```ts
get catalog(): Catalog;
```

Gets the Catalog instance for discovering and managing models.

##### Returns

[`Catalog`](Catalog.md)

The Catalog instance.

***

### isWebServiceRunning

#### Get Signature

```ts
get isWebServiceRunning(): boolean;
```

Whether the web service is currently running.

##### Returns

`boolean`

***

### urls

#### Get Signature

```ts
get urls(): string[];
```

Gets the URLs where the web service is listening.
Returns an empty array if the web service is not running.

##### Returns

`string`[]

An array of URLs.

## Methods

### createResponsesClient()

```ts
createResponsesClient(modelId?): ResponsesClient;
```

Creates a ResponsesClient for interacting with the Responses API.
The web service must be started first via `startWebService()`.

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `modelId?` | `string` | Optional default model ID for requests. |

#### Returns

[`ResponsesClient`](ResponsesClient.md)

A ResponsesClient instance.

#### Throws

Error - If the web service is not running.

***

### downloadAndRegisterEps()

```ts
downloadAndRegisterEps(): void;
```

Download and register execution providers.
Only relevant when using the WinML variant. On non-WinML builds this is a no-op.

Call this after initialization to trigger EP download before accessing the catalog,
so that hardware-accelerated execution providers (e.g. QNN for NPU) are available
when listing and loading models.

#### Returns

`void`

#### Throws

Error - If execution provider download or registration fails.

***

### startWebService()

```ts
startWebService(): void;
```

Starts the local web service.
Use the `urls` property to retrieve the bound addresses after the service has started.
If no listener address is configured, the service defaults to `127.0.0.1:0` (binding to a random ephemeral port).

#### Returns

`void`

#### Throws

Error - If starting the service fails.

***

### stopWebService()

```ts
stopWebService(): void;
```

Stops the local web service.

#### Returns

`void`

#### Throws

Error - If stopping the service fails.

***

### create()

```ts
static create(config): FoundryLocalManager;
```

Creates the FoundryLocalManager singleton with the provided configuration.

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `config` | [`FoundryLocalConfig`](../README.md#foundrylocalconfig) | The configuration settings for the SDK (plain object). |

#### Returns

`FoundryLocalManager`

The initialized FoundryLocalManager instance.

#### Example

```typescript
const manager = FoundryLocalManager.create({
  appName: 'MyApp',
  logLevel: 'info'
});
```
