import { FoundryLocalManager, createImageContent } from 'foundry-local-sdk';

// ── Configuration ──────────────────────────────────────────────────────────
const MODEL_ALIAS = 'qwen3.5-4b';

// ── CLI Arguments ──────────────────────────────────────────────────────────
const args = process.argv.slice(2);
let textInput = null;
let imagePath = null;
let checkCache = false;

for (let i = 0; i < args.length; i++) {
    if (args[i] === '--image' && i + 1 < args.length) imagePath = args[++i];
    else if (args[i] === '--check-cache') checkCache = true;
    else if (!textInput) textInput = args[i];
}

if (!textInput && !checkCache) {
    console.log('Usage: node app.js "your prompt" [--image path/or/url] [--check-cache]');
    console.log('       node app.js "What is quantum computing?"');
    console.log('       node app.js "Describe this image" --image photo.jpg');
    console.log('       node app.js --check-cache');
    process.exit(1);
}

// ── Initialize SDK ─────────────────────────────────────────────────────────
console.log('Initializing Foundry Local SDK...');
const manager = FoundryLocalManager.create({ appName: 'responses_api_qwen3', logLevel: 'info' });
console.log('✓ SDK initialized');

// ── Check Cache ────────────────────────────────────────────────────────────
if (checkCache) {
    const cached = await manager.catalog.getCachedModels();
    const uniqueAliases = [...new Set(cached.map(m => m.alias))];
    if (uniqueAliases.length === 0) console.log('\nNo models in cache.');
    else { console.log(`\nCached models (${uniqueAliases.length}):`); uniqueAliases.forEach(alias => console.log(`  - ${alias}`)); }
    const found = cached.some(m => m.alias === MODEL_ALIAS || m.id === MODEL_ALIAS);
    console.log(`\n${MODEL_ALIAS} is ${found ? '✓ cached' : '✗ NOT cached'}`);
    process.exit(0);
}

// ── Setup Execution Providers ──────────────────────────────────────────────
const eps = manager.discoverEps();
if (eps.length > 0) {
    console.log('\nDownloading execution providers...');
    await manager.downloadAndRegisterEps((name, pct) => {
        process.stdout.write(`\r  ${name.padEnd(30)} ${pct.toFixed(1).padStart(5)}%`);
    });
    process.stdout.write('\n');
}

// ── Load Model ─────────────────────────────────────────────────────────────
let model;
try { model = await manager.catalog.getModel(MODEL_ALIAS); } catch {
    const cached = await manager.catalog.getCachedModels();
    model = cached.find(m => m.alias === MODEL_ALIAS || m.id === MODEL_ALIAS);
    if (model) console.log(`⚠ Model '${MODEL_ALIAS}' not in catalog, using cached version`);
}
if (!model) { console.error(`✗ Model '${MODEL_ALIAS}' not found in catalog or cache.`); process.exit(1); }

// Select CPU variant if multiple variants exist
if (model.variants?.length > 1) {
    console.log(`\nModel variants:`);
    model.variants.forEach(v => console.log(`  - ${v.id} (cached: ${v.isCached})`));
    const cpu = model.variants.find(v => v.id.includes('cpu'));
    if (cpu) { model.selectVariant(cpu); console.log(`✓ Selected CPU variant: ${cpu.id}`); }
}

if (!model.isCached) {
    console.log(`\nDownloading model ${MODEL_ALIAS}...`);
    await model.download(p => process.stdout.write(`\rDownloading... ${p.toFixed(2)}%`));
    console.log('\n✓ Model downloaded');
} else {
    console.log(`✓ Model ${MODEL_ALIAS} found in cache`);
}

console.log(`Loading model ${MODEL_ALIAS}...`);
await model.load();
console.log('✓ Model loaded');

// ── Start Web Service & Create Client ──────────────────────────────────────
manager.startWebService();
const client = manager.createResponsesClient(model.id);
console.log(`✓ Web service started at ${client['baseUrl']}`);
client.settings.temperature = 0.7;
client.settings.maxOutputTokens = model.info.maxOutputTokens || 512;

// ── Run Inference ──────────────────────────────────────────────────────────
if (imagePath) console.log(`\nImage: ${imagePath}`);
console.log(`Prompt: ${textInput}\n`);
console.log('--- Response ---');

const input = [{ type: 'message', role: 'user', content: [] }];

if (imagePath) {
    input[0].content.push(await createImageContent(imagePath));
}
input[0].content.push({ type: 'input_text', text: textInput });

await client.createStreaming(input, (event) => {
    if (event.type === 'response.output_text.delta') process.stdout.write(event.delta);
    if (event.type === 'response.failed') console.error('\n[ERROR]', JSON.stringify(event.response?.error || event));
    if (event.type === 'response.completed') console.log(`\n[Status: ${event.response?.status}]`);
});

console.log('\n');

// ── Cleanup ────────────────────────────────────────────────────────────────
console.log('Unloading model...');
await model.unload();
console.log('✓ Done');
