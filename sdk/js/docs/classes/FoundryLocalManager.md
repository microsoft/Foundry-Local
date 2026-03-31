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
discoverEps(): {
  IsRegistered: boolean;
  Name: string;
}[];
```

Discovers the execution providers available for download and registration.

#### Returns

\{
  `IsRegistered`: `boolean`;
  `Name`: `string`;
\}[]

An array of EP info objects with Name and IsRegistered status.

***

### ensureEpsDownloaded()

```ts
ensureEpsDownloaded(names?, progressCallback?): Promise<void>;
```

Ensures that the necessary execution providers (EPs) are downloaded and registered.
If EPs are already downloaded, this returns immediately. Otherwise it waits for
any in-progress or new downloads to complete.

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `names?` | `string`[] | Optional array of EP names to download. If omitted, all discoverable EPs are downloaded. |
| `progressCallback?` | (`name`, `percent`) => `void` | Optional callback receiving per-EP progress updates. Each update has `name` (EP name) and `percent` (0-100). |

#### Returns

`Promise`\<`void`\>

A promise that resolves when all EPs are ready.

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
