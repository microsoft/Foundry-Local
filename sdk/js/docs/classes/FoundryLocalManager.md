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

#### Call Signature

```ts
downloadAndRegisterEps(): Promise<EpDownloadResult>;
```

Downloads and registers execution providers.

##### Returns

`Promise`\<[`EpDownloadResult`](../README.md#epdownloadresult)\>

A promise that resolves with an EpDownloadResult describing the outcome.

#### Call Signature

```ts
downloadAndRegisterEps(names): Promise<EpDownloadResult>;
```

Downloads and registers execution providers.

##### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `names` | `string`[] | Array of EP names to download. |

##### Returns

`Promise`\<[`EpDownloadResult`](../README.md#epdownloadresult)\>

A promise that resolves with an EpDownloadResult describing the outcome.

#### Call Signature

```ts
downloadAndRegisterEps(progressCallback): Promise<EpDownloadResult>;
```

Downloads and registers execution providers, reporting progress.

##### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `progressCallback` | (`epName`, `percent`) => `void` | Callback invoked with (epName, percent) as each EP downloads. Percent is 0-100. |

##### Returns

`Promise`\<[`EpDownloadResult`](../README.md#epdownloadresult)\>

A promise that resolves with an EpDownloadResult describing the outcome.

#### Call Signature

```ts
downloadAndRegisterEps(names, progressCallback): Promise<EpDownloadResult>;
```

Downloads and registers execution providers, reporting progress.

##### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `names` | `string`[] | Array of EP names to download. |
| `progressCallback` | (`epName`, `percent`) => `void` | Callback invoked with (epName, percent) as each EP downloads. Percent is 0-100. |

##### Returns

`Promise`\<[`EpDownloadResult`](../README.md#epdownloadresult)\>

A promise that resolves with an EpDownloadResult describing the outcome.

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
