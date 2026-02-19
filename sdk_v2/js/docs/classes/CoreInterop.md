[**@prathikrao/foundry-local-sdk**](../README.md)

***

[@prathikrao/foundry-local-sdk](../globals.md) / CoreInterop

# Class: CoreInterop

Defined in: [detail/coreInterop.ts:27](https://github.com/microsoft/Foundry-Local/blob/69c510db89a256a06600e9feab0639030838b463/sdk_v2/js/src/detail/coreInterop.ts#L27)

**`Internal`**

## Constructors

### Constructor

> **new CoreInterop**(`config`): `CoreInterop`

Defined in: [detail/coreInterop.ts:61](https://github.com/microsoft/Foundry-Local/blob/69c510db89a256a06600e9feab0639030838b463/sdk_v2/js/src/detail/coreInterop.ts#L61)

#### Parameters

##### config

[`Configuration`](Configuration.md)

#### Returns

`CoreInterop`

## Methods

### executeCommand()

> **executeCommand**(`command`, `params?`): `string`

Defined in: [detail/coreInterop.ts:82](https://github.com/microsoft/Foundry-Local/blob/69c510db89a256a06600e9feab0639030838b463/sdk_v2/js/src/detail/coreInterop.ts#L82)

#### Parameters

##### command

`string`

##### params?

`any`

#### Returns

`string`

***

### executeCommandStreaming()

> **executeCommandStreaming**(`command`, `params`, `callback`): `Promise`\<`void`\>

Defined in: [detail/coreInterop.ts:115](https://github.com/microsoft/Foundry-Local/blob/69c510db89a256a06600e9feab0639030838b463/sdk_v2/js/src/detail/coreInterop.ts#L115)

#### Parameters

##### command

`string`

##### params

`any`

##### callback

(`chunk`) => `void`

#### Returns

`Promise`\<`void`\>
