import { FoundryLocalManager } from '@prathikrao/foundry-local-sdk';
import { OpenAI } from 'openai';

// Initialize the Foundry Local SDK
console.log('Initializing Foundry Local SDK...');

const endpointUrl = 'http://localhost:5764';

const manager = FoundryLocalManager.create({
    appName: 'foundry_local_samples',
    logLevel: 'info',
    webServiceUrls: endpointUrl
});
console.log('✓ SDK initialized successfully');

// Get the model object
const modelAlias = 'qwen2.5-0.5b'; // Using an available model from the list above
const model = await manager.catalog.getModel(modelAlias);

// Download the model
console.log(`\nDownloading model ${modelAlias}...`);
model.download();
console.log('✓ Model downloaded');

// Load the model
console.log(`\nLoading model ${modelAlias}...`);
model.load();
console.log('✓ Model loaded');

// Start the web service
console.log('\nStarting web service...');
manager.startWebService();
console.log('✓ Web service started');

const openai = new OpenAI({
    baseURL: endpointUrl + '/v1',
    apiKey: 'notneeded',
});

// Example chat completion
console.log('\nTesting chat completion with OpenAI client...');
const response = await openai.chat.completions.create({
    model: model.id,
    messages: [
    {
        role: "user",
        content: "What is the golden ratio?",
    },
    ],
});

console.log(response.choices[0].message.content);

// Tidy up
console.log('Unloading model and stopping web service...');
await model.unload();
await manager.stopWebService();
console.log(`✓ Model unloaded and web service stopped`);
