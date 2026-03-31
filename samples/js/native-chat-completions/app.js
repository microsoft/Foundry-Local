import { FoundryLocalManager } from 'foundry-local-sdk';

// Initialize the Foundry Local SDK
console.log('Initializing Foundry Local SDK...');

const manager = FoundryLocalManager.create({
    appName: 'foundry_local_samples',
    logLevel: 'info'
});
console.log('✓ SDK initialized successfully');

// Discover available execution providers
console.log('\nDiscovering execution providers...');
const eps = manager.discoverEps();
console.log(`Found ${eps.length} EP(s):`);
if (eps.length > 0) {
    const maxNameLen = Math.max(...eps.map(ep => ep.Name.length));
    eps.forEach(ep => {
        console.log(`  ${ep.Name.padEnd(maxNameLen)}  (registered: ${ep.IsRegistered})`);
    });

    // Download and register all discovered EPs with per-EP progress
    const epNames = eps.map(ep => ep.Name);
    console.log(`\nDownloading ${epNames.length} execution provider(s)...`);
    let currentEp = '';
    await manager.ensureEpsDownloaded(epNames, (name, percent) => {
        if (name !== currentEp) {
            if (currentEp.length > 0) process.stdout.write('\n');
            currentEp = name;
        }
        const barLen = 30;
        const filled = Math.round((percent / 100) * barLen);
        const bar = '█'.repeat(filled) + '░'.repeat(barLen - filled);
        process.stdout.write(`\r  ${name.padEnd(maxNameLen)}  [${bar}] ${percent.toFixed(1)}%`);
    });
    console.log('\n✓ All execution providers ready');
}

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
for await (const chunk of chatClient.completeStreamingChat(
    [{ role: 'user', content: 'Write a short poem about programming.' }]
)) {
    const content = chunk.choices?.[0]?.message?.content;
    if (content) {
        process.stdout.write(content);
    }
}
console.log('\n');

// Unload the model
console.log('Unloading model...');
await model.unload();
console.log(`✓ Model unloaded`);
    