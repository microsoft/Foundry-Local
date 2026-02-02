[**@prathikrao/foundry-local-sdk**](../README.md)

***

[@prathikrao/foundry-local-sdk](../globals.md) / IModel

# Interface: IModel

Defined in: [imodel.ts:4](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/imodel.ts#L4)

## Accessors

### alias

#### Get Signature

> **get** **alias**(): `string`

Defined in: [imodel.ts:6](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/imodel.ts#L6)

##### Returns

`string`

***

### id

#### Get Signature

> **get** **id**(): `string`

Defined in: [imodel.ts:5](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/imodel.ts#L5)

##### Returns

`string`

***

### isCached

#### Get Signature

> **get** **isCached**(): `boolean`

Defined in: [imodel.ts:7](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/imodel.ts#L7)

##### Returns

`boolean`

***

### path

#### Get Signature

> **get** **path**(): `string`

Defined in: [imodel.ts:11](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/imodel.ts#L11)

##### Returns

`string`

## Methods

### createAudioClient()

> **createAudioClient**(): [`AudioClient`](../classes/AudioClient.md)

Defined in: [imodel.ts:17](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/imodel.ts#L17)

#### Returns

[`AudioClient`](../classes/AudioClient.md)

***

### createChatClient()

> **createChatClient**(): [`ChatClient`](../classes/ChatClient.md)

Defined in: [imodel.ts:16](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/imodel.ts#L16)

#### Returns

[`ChatClient`](../classes/ChatClient.md)

***

### download()

> **download**(`progressCallback?`): `void`

Defined in: [imodel.ts:10](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/imodel.ts#L10)

#### Parameters

##### progressCallback?

(`progress`) => `void`

#### Returns

`void`

***

### isLoaded()

> **isLoaded**(): `Promise`\<`boolean`\>

Defined in: [imodel.ts:8](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/imodel.ts#L8)

#### Returns

`Promise`\<`boolean`\>

***

### load()

> **load**(): `Promise`\<`void`\>

Defined in: [imodel.ts:12](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/imodel.ts#L12)

#### Returns

`Promise`\<`void`\>

***

### removeFromCache()

> **removeFromCache**(): `void`

Defined in: [imodel.ts:13](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/imodel.ts#L13)

#### Returns

`void`

***

### unload()

> **unload**(): `Promise`\<`void`\>

Defined in: [imodel.ts:14](https://github.com/microsoft/Foundry-Local/blob/03d8abe494b495f2cafc516bcebbbb66a9b6662f/sdk_v2/js/src/imodel.ts#L14)

#### Returns

`Promise`\<`void`\>
