[**@prathikrao/foundry-local-sdk**](../README.md)

***

[@prathikrao/foundry-local-sdk](../globals.md) / FoundryLocalConfig

# Interface: FoundryLocalConfig

Defined in: [configuration.ts:5](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/configuration.ts#L5)

Configuration options for the Foundry Local SDK.
Use a plain object with these properties to configure the SDK.

## Properties

### additionalSettings?

> `optional` **additionalSettings**: `object`

Defined in: [configuration.ts:61](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/configuration.ts#L61)

Additional settings to pass to the core.
Optional. Internal use only.

#### Index Signature

\[`key`: `string`\]: `string`

***

### appDataDir?

> `optional` **appDataDir**: `string`

Defined in: [configuration.ts:16](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/configuration.ts#L16)

The directory where application data should be stored.
Optional. Defaults to `{user_home}/.{appName}`.

***

### appName

> **appName**: `string`

Defined in: [configuration.ts:10](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/configuration.ts#L10)

**REQUIRED** The name of the application using the SDK.
Used for identifying the application in logs and telemetry.

***

### libraryPath?

> `optional` **libraryPath**: `string`

Defined in: [configuration.ts:55](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/configuration.ts#L55)

The path to the directory containing the native Foundry Local Core libraries.
Optional. This directory must contain `Microsoft.AI.Foundry.Local.Core`, `onnxruntime`, and `onnxruntime-genai` binaries.
If not provided, the SDK attempts to discover them in standard locations.

***

### logLevel?

> `optional` **logLevel**: `"trace"` \| `"debug"` \| `"info"` \| `"warn"` \| `"error"` \| `"fatal"`

Defined in: [configuration.ts:35](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/configuration.ts#L35)

The logging level for the SDK.
Optional. Valid values: 'trace', 'debug', 'info', 'warn', 'error', 'fatal'.
Defaults to 'warn'.

***

### logsDir?

> `optional` **logsDir**: `string`

Defined in: [configuration.ts:28](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/configuration.ts#L28)

The directory where log files are written.
Optional. Defaults to `{appDataDir}/logs`.

***

### modelCacheDir?

> `optional` **modelCacheDir**: `string`

Defined in: [configuration.ts:22](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/configuration.ts#L22)

The directory where models are downloaded and cached.
Optional. Defaults to `{appDataDir}/cache/models`.

***

### serviceEndpoint?

> `optional` **serviceEndpoint**: `string`

Defined in: [configuration.ts:48](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/configuration.ts#L48)

The external URL if the web service is running in a separate process.
Optional. This is used to connect to an existing service instance.

***

### webServiceUrls?

> `optional` **webServiceUrls**: `string`

Defined in: [configuration.ts:42](https://github.com/microsoft/Foundry-Local/blob/432b9d46cb2462148eda49b66fcefea597699b0d/sdk_v2/js/src/configuration.ts#L42)

The URL(s) for the local web service to bind to.
Optional. Multiple URLs can be separated by semicolons.
Example: "http://127.0.0.1:8080"
