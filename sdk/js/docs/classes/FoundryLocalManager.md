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

### discoverEps()

```ts
discoverEps(): EpInfo[];
```

Discovers available execution providers (EPs) and their registration status.

#### Returns

[`EpInfo`](../README.md#epinfo)[]

An array of EpInfo describing each available EP.

***

### downloadAndRegisterEps()

```ts
downloadAndRegisterEps(names?): EpDownloadResult;
```

Downloads and registers execution providers. This is a blocking call.

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `names?` | `string`[] | Optional array of EP names to download. If omitted, all available EPs are downloaded. |

#### Returns

[`EpDownloadResult`](../README.md#epdownloadresult)

An EpDownloadResult with the outcome of the operation.

***

### downloadAndRegisterEpsWithProgress()

```ts
downloadAndRegisterEpsWithProgress(names, progressCallback): Promise<void>;
```

Downloads and registers execution providers with per-EP progress reporting.

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `names` | `string`[] \| `undefined` | Optional array of EP names to download. If omitted, all available EPs are downloaded. |
| `progressCallback` | (`epName`, `percent`) => `void` | Called with (epName, percent) as each EP downloads. Percent is 0-100. |

#### Returns

`Promise`\<`void`\>

A promise that resolves when all downloads complete.

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
