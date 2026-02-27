import { FoundryLocalManager } from 'foundry-local-sdk';

// Initialize the Foundry Local SDK
console.log('Initializing Foundry Local SDK...');

const manager = FoundryLocalManager.create({
    appName: 'foundry_local_samples',
    logLevel: 'info'
});
console.log('✓ SDK initialized successfully');

// Get the model variant
const modelAlias = 'whisper-tiny';
const model = await manager.catalog.getModel(modelAlias);
const variant = model.variants[0];
console.log(`Using model: ${variant.id}`);

// Download the model
console.log(`\nDownloading model ${modelAlias}...`);
await variant.download((progress) => {
    process.stdout.write(`\rDownloading... ${progress.toFixed(2)}%`);
});
console.log('\n✓ Model downloaded');

// Load the model
console.log(`\nLoading model ${modelAlias}...`);
await variant.load();
console.log('✓ Model loaded');

// Create audio client
console.log('\nCreating audio client...');
const audioClient = variant.createAudioClient();
console.log('✓ Audio client created');

// Example audio transcription
console.log('\nTesting audio transcription...');
const transcription = await audioClient.transcribe('./Recording.mp3');

console.log('\nAudio transcription result:');
console.log(transcription.text);

// Unload the model
console.log('Unloading model...');
await variant.unload();
console.log(`✓ Model unloaded`);
