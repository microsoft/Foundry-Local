[**@prathikrao/foundry-local-sdk**](../README.md)

***

[@prathikrao/foundry-local-sdk](../globals.md) / CoreInterop

# Class: CoreInterop

Defined in: [detail/coreInterop.ts:27](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/detail/coreInterop.ts#L27)

**`Internal`**

## Constructors

### Constructor

> **new CoreInterop**(`config`): `CoreInterop`

Defined in: [detail/coreInterop.ts:60](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/detail/coreInterop.ts#L60)

#### Parameters

##### config

[`Configuration`](Configuration.md)

#### Returns

`CoreInterop`

## Methods

### executeCommand()

> **executeCommand**(`command`, `params?`): `string`

Defined in: [detail/coreInterop.ts:81](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/detail/coreInterop.ts#L81)

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

Defined in: [detail/coreInterop.ts:114](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/detail/coreInterop.ts#L114)

#### Parameters

##### command

`string`

##### params

`any`

##### callback

(`chunk`) => `void`

#### Returns

`Promise`\<`void`\>
