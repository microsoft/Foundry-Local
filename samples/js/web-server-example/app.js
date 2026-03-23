import { FoundryLocalManager } from 'foundry-local-sdk';
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
const modelAlias = 'qwen2.5-0.5b';
const model = await manager.catalog.getModel(modelAlias);

// Check cache before downloading — skip download if model is already cached
if (!model.isCached) {
    console.log(`\nModel "${modelAlias}" not found in cache. Downloading...`);
    await model.download((progress) => {
        const barWidth = 30;
        const filled = Math.round((progress / 100) * barWidth);
        const bar = '█'.repeat(filled) + '░'.repeat(barWidth - filled);
        process.stdout.write(`\rDownloading: [${bar}] ${progress.toFixed(1)}%`);
        if (progress >= 100) process.stdout.write('\n');
    });
    console.log('✓ Model downloaded');
} else {
    console.log(`\n✓ Model "${modelAlias}" already cached — skipping download`);
}

// Load the model into memory
console.log(`Loading model ${modelAlias}...`);
await model.load();
console.log('✓ Model loaded and ready');

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
manager.stopWebService();
console.log(`✓ Model unloaded and web service stopped`);
