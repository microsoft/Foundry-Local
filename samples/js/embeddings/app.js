// <complete_code>
// <imports>
import { FoundryLocalManager } from 'foundry-local-sdk';
// </imports>

// Initialize the Foundry Local SDK
console.log('Initializing Foundry Local SDK...');

// <init>
const manager = FoundryLocalManager.create({
    appName: 'foundry_local_samples',
    logLevel: 'info'
});
// </init>
console.log('✓ SDK initialized successfully');

// Discover available execution providers and their registration status.
const eps = manager.discoverEps();
const maxNameLen = 30;
console.log('\nAvailable execution providers:');
console.log(`  ${'Name'.padEnd(maxNameLen)}  Registered`);
console.log(`  ${'─'.repeat(maxNameLen)}  ──────────`);
for (const ep of eps) {
    console.log(`  ${ep.name.padEnd(maxNameLen)}  ${ep.isRegistered}`);
}

// Download and register all execution providers with per-EP progress.
// EP packages include dependencies and may be large.
// Download is only required again if a new version of the EP is released.
console.log('\nDownloading execution providers:');
if (eps.length > 0) {
    let currentEp = '';
    await manager.downloadAndRegisterEps((epName, percent) => {
        if (epName !== currentEp) {
            if (currentEp !== '') {
                process.stdout.write('\n');
            }
            currentEp = epName;
        }
        process.stdout.write(`\r  ${epName.padEnd(maxNameLen)}  ${percent.toFixed(1).padStart(5)}%`);
    });
    process.stdout.write('\n');
} else {
    console.log('No execution providers to download.');
}

// <model_setup>
// Get an embedding model
const modelAlias = 'qwen3-embedding-0.6b';
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

// <single_embedding>
// Create embedding client
console.log('\nCreating embedding client...');
const embeddingClient = model.createEmbeddingClient();
console.log('✓ Embedding client created');

// Generate a single embedding
console.log('\n--- Single Embedding ---');
const response = await embeddingClient.generateEmbedding(
    'The quick brown fox jumps over the lazy dog'
);

const embedding = response.data[0].embedding;
console.log(`Dimensions: ${embedding.length}`);
console.log(`First 5 values: [${embedding.slice(0, 5).map(v => v.toFixed(6)).join(', ')}]`);
// </single_embedding>

// <batch_embedding>
// Generate embeddings for multiple inputs
console.log('\n--- Batch Embeddings ---');
const batchResponse = await embeddingClient.generateEmbeddings([
    'Machine learning is a subset of artificial intelligence',
    'The capital of France is Paris',
    'Rust is a systems programming language'
]);

console.log(`Number of embeddings: ${batchResponse.data.length}`);
for (let i = 0; i < batchResponse.data.length; i++) {
    console.log(`  [${i}] Dimensions: ${batchResponse.data[i].embedding.length}`);
}
// </batch_embedding>

// <cleanup>
// Unload the model
console.log('\nUnloading model...');
await model.unload();
console.log('✓ Model unloaded');
// </cleanup>
// </complete_code>
