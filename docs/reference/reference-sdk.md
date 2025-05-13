# Foundry Local Control Plane SDK Reference

> **⚠️ SDK UNDER DEVELOPMENT**
>
> This SDK is actively being developed and may introduce breaking changes without prior notice. We recommend monitoring the changelog for updates before building production applications.


The Foundry Local Control Plane SDK simplifies AI model management in local environments by providing control-plane operations separate from data-plane inferencing code. This reference documents the SDK implementation for Python and JavaScript.

## Python SDK Reference

### Installation

Install the Python package:

```bash
pip install foundry-manager
```

### FoundryManager Class

The `FoundryManager` class provides methods to manage models, cache, and the Foundry Local service.

#### Initialization

```python
from foundry_manager import FoundryManager

# Initialize and optionally bootstrap with a model
manager = FoundryManager(model_id_or_alias=None, bootstrap=True)
```

- `model_id_or_alias`: (optional) Model ID or alias to download and load at startup.
- `bootstrap`: (default True) If True, starts the service if not running and loads the model if provided.

### Service Management

| Method                | Signature                  | Description                                      |
|-----------------------|---------------------------|--------------------------------------------------|
| `is_service_running()`| `() -> bool`              | Checks if the Foundry Local service is running.   |
| `start_service()`     | `() -> None`              | Starts the Foundry Local service.                |
| `service_uri`         | `@property -> str`        | Returns the service URI.                         |
| `endpoint`            | `@property -> str`        | Returns the service endpoint.                    |
| `api_key`             | `@property -> str`        | Returns the API key (from env or default).       |

### Catalog Management

| Method                    | Signature                                         | Description                                      |
|---------------------------|---------------------------------------------------|--------------------------------------------------|
| `list_catalog_models()`   | `() -> list[FoundryModelInfo]`                    | Lists all available models in the catalog.        |
| `refresh_catalog()`       | `() -> None`                                      | Refreshes the model catalog.                     |
| `get_model_info()`        | `(model_alias_or_id: str, raise_on_not_found=False) -> FoundryModelInfo or None` | Gets model info by alias or ID.                  |

### Cache Management

| Method                    | Signature                                         | Description                                      |
|---------------------------|---------------------------------------------------|--------------------------------------------------|
| `get_cache_location()`    | `() -> str`                                       | Returns the model cache directory path.           |
| `list_local_models()`     | `() -> list[FoundryModelInfo]`                    | Lists models downloaded to the local cache.       |

### Model Management

| Method                        | Signature                                                                 | Description                                      |
|-------------------------------|---------------------------------------------------------------------------|--------------------------------------------------|
| `download_model()`            | `(model_alias_or_id: str, token: str = None, force: bool = False) -> FoundryModelInfo` | Downloads a model to the local cache.            |
| `load_model()`                | `(model_alias_or_id: str, ttl: int = 600) -> FoundryModelInfo`            | Loads a model into the inference server.         |
| `unload_model()`              | `(model_alias_or_id: str, force: bool = False) -> None`                   | Unloads a model from the inference server.       |
| `list_loaded_models()`        | `() -> list[FoundryModelInfo]`                                            | Lists all models currently loaded in the service.|

#### Example Usage

```python
from foundry_local import FoundryManager

manager = FoundryManager()

# List available models in the catalog
catalog = manager.list_catalog_models()

# Download and load a model
manager.download_model("DeepSeek-R1-Distill-Qwen-1.5B")
manager.load_model("DeepSeek-R1-Distill-Qwen-1.5B")

# List models in cache
local_models = manager.list_local_models()

# List loaded models
loaded = manager.list_loaded_models()

# Unload a model
manager.unload_model("DeepSeek-R1-Distill-Qwen-1.5B")
```

## JavaScript SDK Reference

### Installation

Install the package from npm:

```bash
npm install foundry-manager
```

### FoundryManager Class

The `FoundryManager` class lets you manage models, control the cache, and interact with the Foundry Local service in both browser and Node.js environments.

#### Initialization

```js
import FoundryManager from 'foundry-manager'

const manager = new FoundryManager()
```

Available options:
- `serviceUrl`: Base URL of the Foundry Local service
- `fetch`: (optional) Custom fetch implementation for environments like Node.js


### Service Management

| Method                | Signature                  | Description                                      |
|-----------------------|---------------------------|--------------------------------------------------|
| `init()`              | `(modelAliasOrId?: string) => Promise<void>` | Initializes the SDK and optionally loads a model. |
| `isServiceRunning()`  | `() => Promise<boolean>`  | Checks if the Foundry Local service is running.   |
| `startService()`      | `() => Promise<void>`     | Starts the Foundry Local service.                |
| `serviceUrl`          | `string`                  | The base URL of the Foundry Local service.        |
| `endpoint`            | `string`                  | The API endpoint (serviceUrl + `/v1`).           |
| `apiKey`              | `string`                  | The API key (none).                              |


### Catalog Management

| Method                    | Signature                                                                 | Description                                      |
|---------------------------|---------------------------------------------------------------------------|--------------------------------------------------|
| `listCatalogModels()`     | `() => Promise<FoundryModelInfo[]>`                                       | Lists all available models in the catalog.        |
| `refreshCatalog()`        | `() => Promise<void>`                                                     | Refreshes the model catalog.                     |
| `getModelInfo()`          | `(modelAliasOrId: string, throwOnNotFound = false) => Promise<FoundryModelInfo \| null>` | Gets model info by alias or ID.                  |


### Cache Management

| Method                    | Signature                                         | Description                                      |
|---------------------------|---------------------------------------------------|--------------------------------------------------|
| `getCacheLocation()`      | `() => Promise<string>`                           | Returns the model cache directory path.           |
| `listLocalModels()`       | `() => Promise<FoundryModelInfo[]>`               | Lists models downloaded to the local cache.       |


### Model Management

| Method                        | Signature                                                                 | Description                                      |
|-------------------------------|---------------------------------------------------------------------------|--------------------------------------------------|
| `downloadModel()`             | `(modelAliasOrId: string, force = false, onProgress?) => Promise<FoundryModelInfo>` | Downloads a model to the local cache.            |
| `loadModel()`                 | `(modelAliasOrId: string, ttl = 600) => Promise<FoundryModelInfo>`        | Loads a model into the inference server.         |
| `unloadModel()`               | `(modelAliasOrId: string, force = false) => Promise<void>`                | Unloads a model from the inference server.       |
| `listLoadedModels()`          | `() => Promise<FoundryModelInfo[]>`                                       | Lists all models currently loaded in the service.|


---

## Example Usage

```js
import FoundryManager from 'foundry-manager'

const manager = new FoundryManager()

// Initialize the SDK and optionally load a model
await manager.init('DeepSeek-R1-Distill-Qwen-1.5B')

// Check if the service is running
const isRunning = await manager.isServiceRunning()
console.log(`Service running: ${isRunning}`)

// List available models in the catalog
const catalog = await manager.listCatalogModels()

// Download and load a model
await manager.downloadModel('DeepSeek-R1-Distill-Qwen-1.5B')
await manager.loadModel('DeepSeek-R1-Distill-Qwen-1.5B')

// List models in cache
const localModels = await manager.listLocalModels()

// List loaded models
const loaded = await manager.listLoadedModels()

// Unload a model
await manager.unloadModel('DeepSeek-R1-Distill-Qwen-1.5B')
```

---

## OpenAI-Compatible Usage

Connect to Foundry Local with any OpenAI-compatible client. Here's an example using the `openai` package:

```js
import OpenAI from 'openai';
import FoundryManager from 'foundry-manager'

// Initialize the manager and load a model
const manager = new FoundryManager()
const modelInfo = await manager.loadModel('DeepSeek-R1-Distill-Qwen-1.5B')

// Create an OpenAI client pointing to our local endpoint
const client = new OpenAI({
    apiKey: manager.apiKey,  // Not actually used but required by the client
    baseURL: manager.endpoint
})

// Create a streaming completion
const completion = await client.chat.completions.create({
    model: modelInfo.id,
    messages: [{ role: 'user', content: 'Solve x^2 + 5x + 6 = 0.' }],
    max_tokens: 250,
    stream: true,
})

// Process the streaming response
for await (const chunk of completion) {
    const textChunk = chunk.choices[0]?.delta?.content || ''
    if (textChunk) {
        process.stdout.write(textChunk)
    }
}
```

---

## Browser Usage

The SDK includes a browser-compatible version where you must specify the service URL manually:

```js
import FoundryManager from 'foundry-manager/browser'

const manager = new FoundryManager({ serviceUrl: 'http://localhost:8080' })

// Note: The `init`, `isServiceRunning`, and `startService` methods 
// are not available in the browser version
```

#### Example Usage

```js
import FoundryManager from 'foundry-manager'

const manager = new FoundryManager({ serviceUrl: 'http://localhost:8080' })

// Get all available models
const catalog = await manager.listCatalogModels()

// Download and load a specific model
await manager.downloadModel('DeepSeek-R1-Distill-Qwen-1.5B')
await manager.loadModel('DeepSeek-R1-Distill-Qwen-1.5B')

// View models in your local cache
const localModels = await manager.listLocalModels()

// Check which models are currently loaded
const loaded = await manager.listLoadedModels()

// Unload a model when finished
await manager.unloadModel('DeepSeek-R1-Distill-Qwen-1.5B')
```
