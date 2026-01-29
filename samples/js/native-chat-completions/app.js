import { FoundryLocalManager } from 'foundry-local-sdk';

// Initialize the Foundry Local SDK
console.log('Initializing Foundry Local SDK...');

const manager = FoundryLocalManager.create({
    appName: 'foundry_local_samples',
    logLevel: 'info'
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

// Create chat client
console.log('\nCreating chat client...');
const chatClient = model.createChatClient();
console.log('✓ Chat client created');

// Example chat completion
console.log('\nTesting chat completion...');
const completion = await chatClient.completeChat([
    { role: 'user', content: 'Why is the sky blue?' }
]);

console.log('\nChat completion result:');
console.log(completion.choices[0]?.message?.content);

// Example streaming completion
console.log('\nTesting streaming completion...');
await chatClient.completeStreamingChat(
    [{ role: 'user', content: 'Write a short poem about programming.' }],
    (chunk) => {
        const content = chunk.choices?.[0]?.message?.content;
        if (content) {
            process.stdout.write(content);
        }
    }
);
console.log('\n');

// Unload the model
console.log('Unloading model...');
model.unload();
console.log(`✓ Model unloaded`);
    