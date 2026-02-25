# @prathikrao/foundry-local-sdk

## Enumerations

### DeviceType

#### Enumeration Members

| Enumeration Member | Value |
| ------ | ------ |
| <a id="enumeration-member-cpu"></a> `CPU` | `"CPU"` |
| <a id="enumeration-member-gpu"></a> `GPU` | `"GPU"` |
| <a id="enumeration-member-invalid"></a> `Invalid` | `"Invalid"` |
| <a id="enumeration-member-npu"></a> `NPU` | `"NPU"` |

## Classes

- [AudioClient](classes/AudioClient.md)
- [AudioClientSettings](classes/AudioClientSettings.md)
- [Catalog](classes/Catalog.md)
- [ChatClient](classes/ChatClient.md)
- [ChatClientSettings](classes/ChatClientSettings.md)
- [FoundryLocalManager](classes/FoundryLocalManager.md)
- [Model](classes/Model.md)
- [ModelLoadManager](classes/ModelLoadManager.md)
- [ModelVariant](classes/ModelVariant.md)

## Interfaces

### FoundryLocalConfig

Configuration options for the Foundry Local SDK.
Use a plain object with these properties to configure the SDK.

#### Properties

##### additionalSettings?

```ts
optional additionalSettings: {
[key: string]: string;
};
```

Additional settings to pass to the core.
Optional. Internal use only.

###### Index Signature

```ts
[key: string]: string
```

##### appDataDir?

```ts
optional appDataDir: string;
```

The directory where application data should be stored.
Optional. Defaults to `{user_home}/.{appName}`.

##### appName

```ts
appName: string;
```

**REQUIRED** The name of the application using the SDK.
Used for identifying the application in logs and telemetry.

##### libraryPath?

```ts
optional libraryPath: string;
```

The path to the directory containing the native Foundry Local Core libraries.
Optional. This directory must contain `Microsoft.AI.Foundry.Local.Core`, `onnxruntime`, and `onnxruntime-genai` binaries.
If not provided, the SDK attempts to discover them in standard locations.

##### logLevel?

```ts
optional logLevel: "trace" | "debug" | "info" | "warn" | "error" | "fatal";
```

The logging level for the SDK.
Optional. Valid values: 'trace', 'debug', 'info', 'warn', 'error', 'fatal'.
Defaults to 'warn'.

##### logsDir?

```ts
optional logsDir: string;
```

The directory where log files are written.
Optional. Defaults to `{appDataDir}/logs`.

##### modelCacheDir?

```ts
optional modelCacheDir: string;
```

The directory where models are downloaded and cached.
Optional. Defaults to `{appDataDir}/cache/models`.

##### serviceEndpoint?

```ts
optional serviceEndpoint: string;
```

The external URL if the web service is running in a separate process.
Optional. This is used to connect to an existing service instance.

##### webServiceUrls?

```ts
optional webServiceUrls: string;
```

The URL(s) for the local web service to bind to.
Optional. Multiple URLs can be separated by semicolons.
Example: "http://127.0.0.1:8080"

***

### IModel

#### Accessors

##### alias

###### Get Signature

```ts
get alias(): string;
```

###### Returns

`string`

##### id

###### Get Signature

```ts
get id(): string;
```

###### Returns

`string`

##### isCached

###### Get Signature

```ts
get isCached(): boolean;
```

###### Returns

`boolean`

##### path

###### Get Signature

```ts
get path(): string;
```

###### Returns

`string`

#### Methods

##### createAudioClient()

```ts
createAudioClient(): AudioClient;
```

###### Returns

[`AudioClient`](classes/AudioClient.md)

##### createChatClient()

```ts
createChatClient(): ChatClient;
```

###### Returns

[`ChatClient`](classes/ChatClient.md)

##### download()

```ts
download(progressCallback?): void;
```

###### Parameters

| Parameter | Type |
| ------ | ------ |
| `progressCallback?` | (`progress`) => `void` |

###### Returns

`void`

##### isLoaded()

```ts
isLoaded(): Promise<boolean>;
```

###### Returns

`Promise`\<`boolean`\>

##### load()

```ts
load(): Promise<void>;
```

###### Returns

`Promise`\<`void`\>

##### removeFromCache()

```ts
removeFromCache(): void;
```

###### Returns

`void`

##### unload()

```ts
unload(): Promise<void>;
```

###### Returns

`Promise`\<`void`\>

***

### ModelInfo

#### Properties

##### alias

```ts
alias: string;
```

##### cached

```ts
cached: boolean;
```

##### createdAtUnix

```ts
createdAtUnix: number;
```

##### displayName?

```ts
optional displayName: string | null;
```

##### fileSizeMb?

```ts
optional fileSizeMb: number | null;
```

##### id

```ts
id: string;
```

##### license?

```ts
optional license: string | null;
```

##### licenseDescription?

```ts
optional licenseDescription: string | null;
```

##### maxOutputTokens?

```ts
optional maxOutputTokens: number | null;
```

##### minFLVersion?

```ts
optional minFLVersion: string | null;
```

##### modelSettings?

```ts
optional modelSettings: ModelSettings | null;
```

##### modelType

```ts
modelType: string;
```

##### name

```ts
name: string;
```

##### promptTemplate?

```ts
optional promptTemplate: PromptTemplate | null;
```

##### providerType

```ts
providerType: string;
```

##### publisher?

```ts
optional publisher: string | null;
```

##### runtime?

```ts
optional runtime: Runtime | null;
```

##### supportsToolCalling?

```ts
optional supportsToolCalling: boolean | null;
```

##### task?

```ts
optional task: string | null;
```

##### uri

```ts
uri: string;
```

##### version

```ts
version: number;
```

***

### ModelSettings

#### Properties

##### parameters?

```ts
optional parameters: Parameter[] | null;
```

***

### Parameter

#### Properties

##### name

```ts
name: string;
```

##### value?

```ts
optional value: string | null;
```

***

### PromptTemplate

#### Properties

##### assistant

```ts
assistant: string;
```

##### prompt

```ts
prompt: string;
```

##### system?

```ts
optional system: string | null;
```

##### user?

```ts
optional user: string | null;
```

***

### Runtime

#### Properties

##### deviceType

```ts
deviceType: DeviceType;
```

##### executionProvider

```ts
executionProvider: string;
```
