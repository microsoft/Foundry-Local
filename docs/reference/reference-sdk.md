# Foundry Local Control Plane SDK Reference

> **⚠️ SDK UNDER DEVELOPMENT**
>
> This SDK is actively being developed and may introduce breaking changes without prior notice. We recommend monitoring the changelog for updates before building production applications.


The Foundry Local Control Plane SDK simplifies AI model management in local environments by providing control-plane operations separate from data-plane inferencing code. This reference documents the SDK implementation for Python and JavaScript.

## Python SDK Reference

### Installation

Install the Python package:

```bash
pip install foundry-local-sdk
```

### FoundryLocalManager Class

The `FoundryLocalManager` class provides methods to manage models, cache, and the Foundry Local service.

#### Initialization

```python
from foundry_local import FoundryLocalManager

# Initialize and optionally bootstrap with a model
manager = FoundryLocalManager(alias_or_model_id=None, bootstrap=True)
```

- `alias_or_model_id`: (optional) Alias or Model ID to download and load at startup.
- `bootstrap`: (default True) If True, starts the service if not running and loads the model if provided.

### A note on aliases

Many methods outlined in this reference have an `alias_or_model_id` parameter in the signature. You can pass into the method either an **alias** or **model ID** as a value. Using an alias will:

- Select the *best model* for the available hardware. For example, if a Nvidia CUDA GPU is available, Foundry Local selects the CUDA model. If a supported NPU is available, Foundry Local selects the NPU model.
- Allow you to use a shorter name without needing to remember the model ID.

> [!TIP]
> We recommend passing into the `alias_or_model_id` parameter an **alias** because when you deploy your application, Foundry Local acquires the best model for the end user's machine at run-time.

### Service Management

| Method                | Signature                 | Description                                      |
|-----------------------|---------------------------|--------------------------------------------------|
| `is_service_running()`| `() -> bool`              | Checks if the Foundry Local service is running.  |
| `start_service()`     | `() -> None`              | Starts the Foundry Local service.                |
| `service_uri`         | `@property -> str`        | Returns the service URI.                         |
| `endpoint`            | `@property -> str`        | Returns the service endpoint.                    |
| `api_key`             | `@property -> str`        | Returns the API key (from env or default).       |

### Catalog Management

| Method                    | Signature                                         | Description                                      |
|---------------------------|---------------------------------------------------|--------------------------------------------------|
| `list_catalog_models()`   | `() -> list[FoundryModelInfo]`                    | Lists all available models in the catalog.       |
| `refresh_catalog()`       | `() -> None`                                      | Refreshes the model catalog.                     |
| `get_model_info()`        | `(alias_or_model_id: str, raise_on_not_found=False) -> FoundryModelInfo or None` | Gets model info by alias or ID.                  |

### Cache Management

| Method                    | Signature                                         | Description                                      |
|---------------------------|---------------------------------------------------|--------------------------------------------------|
| `get_cache_location()`    | `() -> str`                                       | Returns the model cache directory path.          |
| `list_cached_models()`    | `() -> list[FoundryModelInfo]`                    | Lists models downloaded to the local cache.      |

### Model Management

| Method                        | Signature                                                                 | Description                                      |
|-------------------------------|---------------------------------------------------------------------------|--------------------------------------------------|
| `download_model()`            | `(alias_or_model_id: str, token: str = None, force: bool = False) -> FoundryModelInfo` | Downloads a model to the local cache.            |
| `load_model()`                | `(alias_or_model_id: str, ttl: int = 600) -> FoundryModelInfo`            | Loads a model into the inference server.         |
| `unload_model()`              | `(alias_or_model_id: str, force: bool = False) -> None`                   | Unloads a model from the inference server.       |
| `list_loaded_models()`        | `() -> list[FoundryModelInfo]`                                            | Lists all models currently loaded in the service.|

## Example Usage

The following code demonstrates how to use the `FoundryManager` class to manage models and interact with the Foundry Local service.

```python
from foundry_local import FoundryLocalManager

# By using an alias, the most suitable model will be selected 
# to your end-user's device.
alias = "phi-3.5-mini"

# Create a FoundryLocalManager instance. This will start the Foundry.
manager = FoundryLocalManager()

# List available models in the catalog
catalog = manager.list_catalog_models()
print(f"Available models in the catalog: {catalog}")

# Download and load a model
model_info = manager.download_model(alias)
model_info = manager.load_model(alias)
print(f"Model info: {model_info}")

# List models in cache
local_models = manager.list_cached_models()
print(f"Models in cache: {local_models}")

# List loaded models
loaded = manager.list_loaded_models()
print(f"Models running in the service: {loaded}")

# Unload a model
manager.unload_model(alias)
```

### Integrate with OpenAI SDK

Install the OpenAI package:

```bash
pip install openai
```

The following code demonstrates how to integrate the `FoundryLocalManager` with the OpenAI SDK to interact with a local model.

```python
import openai
from foundry_local import FoundryLocalManager

# By using an alias, the most suitable model will be downloaded 
# to your end-user's device.
alias = "phi-3.5-mini"

# Create a FoundryLocalManager instance. This will start the Foundry 
# Local service if it is not already running and load the specified model.
manager = FoundryLocalManager(alias)

# The remaining code us es the OpenAI Python SDK to interact with the local model.

# Configure the client to use the local Foundry service
client = openai.OpenAI(
    base_url=manager.endpoint,
    api_key=manager.api_key  # API key is not required for local usage
)

# Set the model to use and generate a streaming response
stream = client.chat.completions.create(
    model=manager.get_model_info(alias).id,
    messages=[{"role": "user", "content": "Why is the sky blue?"}],
    stream=True
)

# Print the streaming response
for chunk in stream:
    if chunk.choices[0].delta.content is not None:
        print(chunk.choices[0].delta.content, end="", flush=True)
```

# JavaScript SDK Reference

### Installation

Install the package from npm:

```bash
npm install foundry-local-sdk
```

### FoundryLocalManager Class

The `FoundryLocalManager` class lets you manage models, control the cache, and interact with the Foundry Local service in both browser and Node.js environments.

#### Initialization

```js
import { FoundryLocalManager } from 'foundry-local-sdk'

const foundryLocalManager = new FoundryLocalManager()
```

Available options:
- `serviceUrl`: Base URL of the Foundry Local service
- `fetch`: (optional) Custom fetch implementation for environments like Node.js

### A note on aliases

Many methods outlined in this reference have an `aliasOrModelId` parameter in the signature. You can pass into the method either an **alias** or **model ID** as a value. Using an alias will:

- Select the *best model* for the available hardware. For example, if a Nvidia CUDA GPU is available, Foundry Local selects the CUDA model. If a supported NPU is available, Foundry Local selects the NPU model.
- Allow you to use a shorter name without needing to remember the model ID.

> [!TIP]
> We recommend passing into the `aliasOrModelId` parameter an **alias** because when you deploy your application, Foundry Local acquires the best model for the end user's machine at run-time.

### Service Management

| Method                | Signature                 | Description                                      |
|-----------------------|---------------------------|--------------------------------------------------|
| `init()`              | `(aliasOrModelId?: string) => Promise<void>` | Initializes the SDK and optionally loads a model. |
| `isServiceRunning()`  | `() => Promise<boolean>`  | Checks if the Foundry Local service is running.  |
| `startService()`      | `() => Promise<void>`     | Starts the Foundry Local service.                |
| `serviceUrl`          | `string`                  | The base URL of the Foundry Local service.       |
| `endpoint`            | `string`                  | The API endpoint (serviceUrl + `/v1`).           |
| `apiKey`              | `string`                  | The API key (none).                              |


### Catalog Management

| Method                    | Signature                                                                 | Description                                      |
|---------------------------|---------------------------------------------------------------------------|--------------------------------------------------|
| `listCatalogModels()`     | `() => Promise<FoundryModelInfo[]>`                                       | Lists all available models in the catalog.       |
| `refreshCatalog()`        | `() => Promise<void>`                                                     | Refreshes the model catalog.                     |
| `getModelInfo()`          | `(aliasOrModelId: string, throwOnNotFound = false) => Promise<FoundryModelInfo \| null>` | Gets model info by alias or ID.   |


### Cache Management

| Method                    | Signature                                         | Description                                      |
|---------------------------|---------------------------------------------------|--------------------------------------------------|
| `getCacheLocation()`      | `() => Promise<string>`                           | Returns the model cache directory path.          |
| `listCachedModels()`      | `() => Promise<FoundryModelInfo[]>`               | Lists models downloaded to the local cache.      |


### Model Management

| Method                        | Signature                                                                 | Description                                      |
|-------------------------------|---------------------------------------------------------------------------|--------------------------------------------------|
| `downloadModel()`             | `(aliasOrModelId: string, token?: string, force = false, onProgress?) => Promise<FoundryModelInfo>` | Downloads a model to the local cache.            |
| `loadModel()`                 | `(aliasOrModelId: string, ttl = 600) => Promise<FoundryModelInfo>`        | Loads a model into the inference server.         |
| `unloadModel()`               | `(aliasOrModelId: string, force = false) => Promise<void>`                | Unloads a model from the inference server.       |
| `listLoadedModels()`          | `() => Promise<FoundryModelInfo[]>`                                       | Lists all models currently loaded in the service.|

## Example Usage

The following code demonstrates how to use the `FoundryLocalManager` class to manage models and interact with the Foundry Local service.

```js
import { FoundryLocalManager } from 'foundry-local-sdk'

// By using an alias, the most suitable model will be downloaded 
// to your end-user's device.
// TIP: You can find a list of available models by running the 
// following command in your terminal: `foundry model list`.
const alias = 'phi-3.5-mini';

const manager = new FoundryLocalManager()

// Initialize the SDK and optionally load a model
const modelInfo = await manager.init(alias)
console.log('Model Info:', modelInfo)

// Check if the service is running
const isRunning = await manager.isServiceRunning()
console.log(`Service running: ${isRunning}`)

// List available models in the catalog
const catalog = await manager.listCatalogModels()

// Download and load a model
await manager.downloadModel(alias)
await manager.loadModel(alias)

// List models in cache
const localModels = await manager.listCachedModels()

// List loaded models
const loaded = await manager.listLoadedModels()

// Unload a model
await manager.unloadModel(alias)
```

---

## Integration with OpenAI Client

Install the OpenAI package:

```bash
npm install openai
```

The following code demonstrates how to integrate the `FoundryLocalManager` with the OpenAI client to interact with a local model.

```js
import { OpenAI } from 'openai'
import { FoundryLocalManager } from 'foundry-local-sdk'

// By using an alias, the most suitable model will be downloaded 
// to your end-user's device.
// TIP: You can find a list of available models by running the 
// following command in your terminal: `foundry model list`.
const alias = 'phi-3.5-mini'

// Create a FoundryLocalManager instance. This will start the Foundry 
// Local service if it is not already running.
const foundryLocalManager = new FoundryLocalManager()

// Initialize the manager with a model. This will download the model 
// if it is not already present on the user's device.
const modelInfo = await foundryLocalManager.init(alias)
console.log('Model Info:', modelInfo)

const openai = new OpenAI({
  baseURL: foundryLocalManager.endpoint,
  apiKey: foundryLocalManager.apiKey,
})

async function streamCompletion() {
  const stream = await openai.chat.completions.create({
    model: modelInfo.id,
    messages: [{ role: 'user', content: 'What is the golden ratio?' }],
    stream: true,
})
  
  for await (const chunk of stream) {
    if (chunk.choices[0]?.delta?.content) {
    process.stdout.write(chunk.choices[0].delta.content)
    }
  }
}
  
streamCompletion()
```

## Browser Usage

The SDK includes a browser-compatible version where you must specify the service URL manually:

```js
import { FoundryLocalManager } from 'foundry-local-sdk/browser'

// Specify the service URL
// Run the Foundry Local service using the CLI: `foundry service start`
// and use the URL from the CLI output
const endpoint = 'ENDPOINT'

const manager = new FoundryLocalManager({serviceUrl: endpoint})

// Note: The `init`, `isServiceRunning`, and `startService` methods 
// are not available in the browser version
```

> [!NOTE]
> The browser version doesn't support the `init`, `isServiceRunning`, and `startService` methods. You must ensure that the Foundry Local service is running before using the SDK in a browser environment. You can start the service using the Foundry Local CLI: `foundry service start`. You can glean the service URL from the CLI output.


#### Example Usage

```js
import { FoundryLocalManager } from 'foundry-local-sdk/browser'

// Specify the service URL
// Run the Foundry Local service using the CLI: `foundry service start`
// and use the URL from the CLI output
const endpoint = 'ENDPOINT'

const manager = new FoundryLocalManager({serviceUrl: endpoint})

const alias = 'phi-3.5-mini'

// Get all available models
const catalog = await manager.listCatalogModels()
console.log('Available models in catalog:', catalog)

// Download and load a specific model
await manager.downloadModel(alias)
await manager.loadModel(alias)

// View models in your local cache
const localModels = await manager.listLocalModels()
console.log('Cached models:', catalog)

// Check which models are currently loaded
const loaded = await manager.listLoadedModels()
console.log('Loaded models in inference service:', loaded)

// Unload a model when finished
await manager.unloadModel(alias)
```
