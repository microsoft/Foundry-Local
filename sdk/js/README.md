# Foundry Local Manager JavaScript SDK
This is a JavaScript Control-Plane SDK for Foundry Local. It provides a simple interface to interact with the Foundry Local API.

## Prerequisites
Foundry Local must be installed and findable in your PATH.

## Getting Started
```bash
npm install foundry-local-sdk
```

## Usage
```js
const { FoundryLocalManager } = require("foundry-local-sdk")

constant alias = "deepseek-r1-1.5b"
const foundryLocalManager = new FoundryLocalManager()

// initialize the SDK with an optional alias or model ID
const modelInfo = await foundryLocalManager.init(alias)
console.log("Model Info:", modelInfo)

// check that the service is running
const isRunning = await foundryLocalManager.isServiceRunning()
console.log(`Service running: ${isRunning}`)

// list all available models in the catalog
const catalogModels = await foundryLocalManager.listCatalogModels()
console.log("Catalog Models:", catalogModels)

// list all downloaded models
const localModels = await foundryLocalManager.listLocalModels()
console.log("Local Models:", localModels)
```

Manually start the service and download a model:
```js
const { FoundryLocalManager } = require("foundry-local-sdk")

const alias = "deepseek-r1-1.5b"
const foundryLocalManager = new FoundryLocalManager()

// start the service
await foundryLocalManager.startService()
// or await foundryLocalManager.init()

// download the model
// the download api also accepts an optional event handler to track the download progress
// it must be of the signature (progress: number) => void
await foundryLocalManager.downloadModel(alias)

// load the model
const modelInfo = await foundryLocalManager.loadModel(alias)
console.log("Model Info:", modelInfo)
```

Use the foundry local endpoint with an OpenAI compatible API client. For example, using the `openai` package:
```js
const { OpenAI } = require('openai')

const client = new OpenAI({
    apiKey: foundryLocalManager.apiKey,
    baseURL: foundryLocalManager.endpoint
})

const completion = await client.chat.completions.create({
    model: modelInfo.id,
    messages: [{"role": "user", "content": "Solve x^2 + 5x + 6 = 0."}],
    max_tokens: 250,
    stream: true,
});
for await (const chunk of completion) {
    const textChunk = chunk.choices[0]?.delta?.content || "";
    if (textChunk) {
        process.stdout.write(textChunk);
    }
}
```

## Browser Usage
The SDK also provides a browser-compatible version. However, it requires you to provide the service URL manually. You can use the `FoundryLocalManager` class in the browser as follows:

```js
import { FoundryLocalManager } from "foundry-local-sdk/browser"

const foundryLocalManager = new FoundryLocalManager({host: "http://localhost:8080"})

// the rest of the code is the same as above other than the init, isServiceRunning, and startService methods
// which are not available in the browser version.
```
