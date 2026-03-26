import { FoundryLocalManager } from 'foundry-local-sdk';

// Initialize the Foundry Local SDK
console.log('Initializing Foundry Local SDK...');

const manager = FoundryLocalManager.create({
    appName: 'foundry_local_samples',
    logLevel: 'info'
});
console.log('✓ SDK initialized successfully');

// Get the model object
const modelAlias = 'whisper-tiny'; // Using an available model from the list above
let model = await manager.catalog.getModel(modelAlias);
console.log(`Using model: ${model.id}`);

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

// Create audio client
console.log('\nCreating audio client...');
const audioClient = model.createAudioClient();
console.log('✓ Audio client created');

// Example audio transcription
console.log('\nTesting audio transcription...');
const transcription = await audioClient.transcribe('./Recording.mp3');

console.log('\nAudio transcription result:');
console.log(transcription.text);
console.log('✓ Audio transcription completed');

// Same example but with streaming transcription using async iteration
console.log('\nTesting streaming audio transcription...');
for await (const result of audioClient.transcribeStreaming('./Recording.mp3')) {
    // Output the intermediate transcription results as they are received without line ending
    process.stdout.write(result.text);
}
console.log('\n✓ Streaming transcription completed');

// Unload the model
console.log('Unloading model...');
await model.unload();
console.log(`✓ Model unloaded`);
