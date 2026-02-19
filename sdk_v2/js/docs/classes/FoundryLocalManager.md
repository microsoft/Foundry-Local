[**@prathikrao/foundry-local-sdk**](../README.md)

***

[@prathikrao/foundry-local-sdk](../globals.md) / FoundryLocalManager

# Class: FoundryLocalManager

Defined in: [foundryLocalManager.ts:12](https://github.com/microsoft/Foundry-Local/blob/69c510db89a256a06600e9feab0639030838b463/sdk_v2/js/src/foundryLocalManager.ts#L12)

The main entry point for the Foundry Local SDK.
Manages the initialization of the core system and provides access to the Catalog and ModelLoadManager.

## Accessors

### catalog

#### Get Signature

> **get** **catalog**(): [`Catalog`](Catalog.md)

Defined in: [foundryLocalManager.ts:52](https://github.com/microsoft/Foundry-Local/blob/69c510db89a256a06600e9feab0639030838b463/sdk_v2/js/src/foundryLocalManager.ts#L52)

Gets the Catalog instance for discovering and managing models.

##### Returns

[`Catalog`](Catalog.md)

The Catalog instance.

***

### urls

#### Get Signature

> **get** **urls**(): `string`[]

Defined in: [foundryLocalManager.ts:61](https://github.com/microsoft/Foundry-Local/blob/69c510db89a256a06600e9feab0639030838b463/sdk_v2/js/src/foundryLocalManager.ts#L61)

Gets the URLs where the web service is listening.
Returns an empty array if the web service is not running.

##### Returns

`string`[]

An array of URLs.

## Methods

### startWebService()

> **startWebService**(): `void`

Defined in: [foundryLocalManager.ts:71](https://github.com/microsoft/Foundry-Local/blob/69c510db89a256a06600e9feab0639030838b463/sdk_v2/js/src/foundryLocalManager.ts#L71)

Starts the local web service.
Use the `urls` property to retrieve the bound addresses after the service has started.
If no listener address is configured, the service defaults to `127.0.0.1:0` (binding to a random ephemeral port).

#### Returns

`void`

#### Throws

Error - If starting the service fails.

***

### stopWebService()

> **stopWebService**(): `void`

Defined in: [foundryLocalManager.ts:84](https://github.com/microsoft/Foundry-Local/blob/69c510db89a256a06600e9feab0639030838b463/sdk_v2/js/src/foundryLocalManager.ts#L84)

Stops the local web service.

#### Returns

`void`

#### Throws

Error - If stopping the service fails.

***

### create()

> `static` **create**(`config`): `FoundryLocalManager`

Defined in: [foundryLocalManager.ts:40](https://github.com/microsoft/Foundry-Local/blob/69c510db89a256a06600e9feab0639030838b463/sdk_v2/js/src/foundryLocalManager.ts#L40)

Creates the FoundryLocalManager singleton with the provided configuration.

#### Parameters

##### config

[`FoundryLocalConfig`](../interfaces/FoundryLocalConfig.md)

The configuration settings for the SDK (plain object).

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
