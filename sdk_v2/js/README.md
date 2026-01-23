# Foundry Local JS SDK

The Foundry Local JS SDK provides a JavaScript/TypeScript interface for interacting with local AI models via the Foundry Local Core. It allows you to discover, download, load, and run inference on models directly on your local machine.

## Installation

To install the SDK, run the following command in your project directory:

```bash
npm install foundry-local-sdk
```

## Usage

### Initialization

Initialize the `FoundryLocalManager` with your configuration.

```typescript
import { FoundryLocalManager } from 'foundry-local-js-sdk';

const manager = FoundryLocalManager.create({
    libraryPath: '/path/to/core/library',
    modelCacheDir: '/path/to/model/cache',
    logLevel: 'info'
});
```

### Discovering Models

Use the `Catalog` to list available models.

```typescript
const catalog = manager.catalog;
const models = catalog.models;

models.forEach(model => {
    console.log(`Model: ${model.alias}`);
});
```

### Loading and Running a Model

```typescript
const model = catalog.getModel('phi-3-mini');

if (model) {
    await model.load();
    
    const chatClient = model.createChatClient();
    const response = await chatClient.completeChat([
        { role: 'user', content: 'Hello, how are you?' }
    ]);
    
    console.log(response.choices[0].message.content);
    
    await model.unload();
}
```

## Documentation

The SDK source code is documented using TSDoc. You can generate the API documentation using TypeDoc.

### Generating Docs

Run the following command to generate the HTML documentation in the `docs` folder:

```bash
npm run docs
```

Open `docs/index.html` in your browser to view the documentation.

## Running Tests

To run the tests, use:

```bash
npm test
```

See `test/README.md` for more details on setting up and running tests.

## Running Examples

The SDK includes an example script demonstrating chat completion. To run it:

1.  Ensure you have the necessary core libraries and a model available (see Tests Prerequisites).
2.  Run the example command:

```bash
npm run example
```

This will execute `examples/chat-completion.ts`.
