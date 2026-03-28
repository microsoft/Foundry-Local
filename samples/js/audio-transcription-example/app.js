import { FoundryLocalManager } from 'foundry-local-sdk';

// Initialize the Foundry Local SDK
console.log('Initializing Foundry Local SDK...');

const manager = FoundryLocalManager.create({
    appName: 'foundry_local_samples',
    logLevel: 'info'
});
console.log('✓ SDK initialized successfully');

// Get the model object
const modelAlias = 'whisper-tiny';
let model = await manager.catalog.getModel(modelAlias);
console.log(`Using model: ${model.id}`);

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
