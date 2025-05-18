# Foundry Local JavaScript SDK

The Foundry Local SDK simplifies AI model management in local environments by providing control-plane operations separate from data-plane inferencing code.

## Prerequisites

Foundry Local must be installed and findable in your PATH.

## Getting Started

```bash
npm install foundry-local-sdk
```

## Usage

The SDK provides a simple interface to interact with the Foundry Local API. You can use it to manage models, check the status of the service, and make requests to the models.

### Bootstrapping

The SDK can *bootstrap* Foundry Local, which will initiate the following sequence:

1. Start the Foundry Local service, if it is not already running.
1. Automatically detect the hardware and software requirements for the model.
1. Download the most performance model for the detected hardware, if it is not already downloaded.
1. Load the model into memory.

To use the SDK with bootstrapping, you can use the following code:

```js
// foundry-local-sdk supports both CommonJS and ES module syntax
// CommonJS
const { FoundryLocalManager } = require('foundry-local-sdk')
// ES module
// import { FoundryLocalManager } from "foundry-local-sdk"

const alias = 'phi-3.5-mini'
const foundryLocalManager = new FoundryLocalManager()

// initialize the SDK with an optional alias or model ID
const modelInfo = await foundryLocalManager.init(alias)
console.log('Model Info:', modelInfo)

// check that the service is running
const isRunning = await foundryLocalManager.isServiceRunning()
console.log(`Service running: ${isRunning}`)

// list all available models in the catalog
const catalogModels = await foundryLocalManager.listCatalogModels()
console.log('Catalog Models:', catalogModels)

// list all downloaded models
const localModels = await foundryLocalManager.listCachedModels()
console.log('Local Models:', localModels)
```

Alternatively, you can use the `FoundryLocalManager` class to manage the service and models manually. This is useful if you want to control the service and models without bootstrapping. For example, you want to present to the end user what is happening in the background.

```js
const { FoundryLocalManager } = require('foundry-local-sdk')

const alias = 'phi-3.5-mini'
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
console.log('Model Info:', modelInfo)
```


## Using the SDK with OpenAI API

Use the foundry local endpoint with an OpenAI compatible API client. For example, install the `openai` package using npm:

```bash
npm install openai
```

Then copy-and-paste the following code into a file called `app.js`:

```js
import { OpenAI } from "openai";
import { FoundryLocalManager } from "foundry-local-sdk";

// By using an alias, the most suitable model will be downloaded 
// to your end-user's device.
// TIP: You can find a list of available models by running the 
// following command in your terminal: `foundry model list`.
const alias = "phi-3.5-mini";

// Create a FoundryLocalManager instance. This will start the Foundry 
// Local service if it is not already running.
const foundryLocalManager = new FoundryLocalManager()

// Initialize the manager with a model. This will download the model 
// if it is not already present on the user's device.
const modelInfo = await foundryLocalManager.init(alias)
console.log("Model Info:", modelInfo)

const openai = new OpenAI({
  baseURL: foundryLocalManager.endpoint,
  apiKey: foundryLocalManager.apiKey,
});

async function streamCompletion() {
    const stream = await openai.chat.completions.create({
      model: modelInfo.id,
      messages: [{ role: "user", content: "What is the golden ratio?" }],
      stream: true,
    });
  
    for await (const chunk of stream) {
      if (chunk.choices[0]?.delta?.content) {
        process.stdout.write(chunk.choices[0].delta.content);
      }
    }
}
  
streamCompletion();
```

Run the application using Node.js:

```bash
node app.js
```

## Browser Usage

The SDK also provides a browser-compatible version. However, it requires you to provide the service URL manually. You can use the `FoundryLocalManager` class in the browser as follows:

```js
import { FoundryLocalManager } from 'foundry-local-sdk/browser'

const foundryLocalManager = new FoundryLocalManager({ host: 'http://localhost:8080' })

// the rest of the code is the same as above other than the init, isServiceRunning, and startService methods
// which are not available in the browser version.
```
