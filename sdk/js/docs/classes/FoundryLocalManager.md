[@prathikrao/foundry-local-sdk](../README.md) / FoundryLocalManager

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
