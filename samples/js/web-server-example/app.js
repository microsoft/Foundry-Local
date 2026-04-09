// <complete_code>
// <imports>
import { FoundryLocalManager } from 'foundry-local-sdk';
import { OpenAI } from 'openai';
// </imports>

// Initialize the Foundry Local SDK
console.log('Initializing Foundry Local SDK...');

const endpointUrl = 'http://localhost:5764';

// <init>
const manager = FoundryLocalManager.create({
    appName: 'foundry_local_samples',
    logLevel: 'info',
    webServiceUrls: endpointUrl
});
// </init>
console.log('✓ SDK initialized successfully');

// Download and register all execution providers.
await manager.downloadAndRegisterEps();

// <model_setup>
// Get the model object
const modelAlias = 'qwen2.5-0.5b'; // Using an available model from the list above
const model = await manager.catalog.getModel(modelAlias);

// Download the model
console.log(`\nDownloading model ${modelAlias}...`);
await model.download((progress) => {
    process.stdout.write(`\rDownloading... ${progress.toFixed(2)}%`);
});
console.log('\n✓ Model downloaded');

// Load the model
console.log(`\nLoading model ${modelAlias}...`);
await model.load();
console.log('✓ Model loaded');
// </model_setup>

// <server_setup>
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
// </server_setup>

// Tidy up
console.log('Unloading model and stopping web service...');
await model.unload();
manager.stopWebService();
console.log(`✓ Model unloaded and web service stopped`);
// </complete_code>
