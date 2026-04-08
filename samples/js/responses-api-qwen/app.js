import { FoundryLocalManager, getOutputText } from 'foundry-local-sdk';
import { readFileSync } from 'fs';
import { resolve, extname } from 'path';

// ── CLI Arguments ───────────────────────────────────────────────────────────
const args = process.argv.slice(2);
let textInput = null;
let imagePath = null;
let checkCache = false;

for (let i = 0; i < args.length; i++) {
    if (args[i] === '--image' && i + 1 < args.length) {
        imagePath = args[++i];
    } else if (args[i] === '--check-cache') {
        checkCache = true;
    } else if (!textInput) {
        textInput = args[i];
    }
}

if (!textInput && !checkCache) {
    console.log('Usage: node app.mjs "your prompt" [--image path/or/url] [--check-cache]');
    console.log('  node app.mjs "What is quantum computing?"');
    console.log('  node app.mjs "Describe this image" --image photo.jpg');
    console.log('  node app.mjs --check-cache');
    process.exit(1);
}

// ── Helpers ─────────────────────────────────────────────────────────────────
const MIME_TYPES = { jpg: 'image/jpeg', jpeg: 'image/jpeg', png: 'image/png', gif: 'image/gif', webp: 'image/webp', bmp: 'image/bmp' };

function getMimeType(filePath) {
    const ext = extname(filePath).toLowerCase().replace('.', '');
    return MIME_TYPES[ext] || 'image/png';
}

function readImageAsDataUri(filePath) {
    const absPath = resolve(filePath);
    const mime = getMimeType(absPath);
    const base64 = readFileSync(absPath).toString('base64');
    return { url: `data:${mime};base64,${base64}`, mediaType: mime };
}

// ── Initialize SDK ──────────────────────────────────────────────────────────
const MODEL_ALIAS = 'qwen3.5-2b';

console.log('Initializing Foundry Local SDK...');
const manager = FoundryLocalManager.create({
    appName: 'responses_api_qwen3',
    logLevel: 'info'
});
console.log('✓ SDK initialized');

// ── Check Cache Mode ────────────────────────────────────────────────────────
if (checkCache) {
    const cachedModels = await manager.catalog.getCachedModels();
    if (cachedModels.length === 0) {
        console.log('\nNo models in cache.');
    } else {
        console.log(`\nCached models (${cachedModels.length}):`);
        for (const m of cachedModels) console.log(`  - ${m.alias}`);
    }
    const isCached = cachedModels.some(m => m.alias === MODEL_ALIAS || m.id === MODEL_ALIAS);
    console.log(`\n${MODEL_ALIAS} is ${isCached ? '✓ cached' : '✗ NOT cached'}`);
    process.exit(0);
}

// ── Setup Execution Providers ───────────────────────────────────────────────
const eps = manager.discoverEps();
if (eps.length > 0) {
    console.log('\nDownloading execution providers...');
    await manager.downloadAndRegisterEps((epName, percent) => {
        process.stdout.write(`\r  ${epName.padEnd(30)}  ${percent.toFixed(1).padStart(5)}%`);
    });
    process.stdout.write('\n');
}

// ── Load Model ──────────────────────────────────────────────────────────────
let model;
try {
    model = await manager.catalog.getModel(MODEL_ALIAS);
} catch {
    const cachedModels = await manager.catalog.getCachedModels();
    model = cachedModels.find(m => m.alias === MODEL_ALIAS || m.id === MODEL_ALIAS);
    if (model) console.log(`⚠ Model '${MODEL_ALIAS}' not in catalog, using cached version`);
}

if (!model) {
    console.error(`✗ Model '${MODEL_ALIAS}' not found in catalog or cache.`);
    process.exit(1);
}

// List and select CPU variant if available
if (model.variants && model.variants.length > 1) {
    console.log(`\nModel variants:`);
    for (const v of model.variants) {
        console.log(`  - ${v.id} (cached: ${v.isCached})`);
    }
    const cpuVariant = model.variants.find(v => v.id.includes('cpu'));
    if (cpuVariant) {
        model.selectVariant(cpuVariant);
        console.log(`✓ Selected CPU variant: ${cpuVariant.id}`);
    }
}

if (!model.isCached) {
    console.log(`\nDownloading model ${MODEL_ALIAS}...`);
    await model.download((p) => process.stdout.write(`\rDownloading... ${p.toFixed(2)}%`));
    console.log('\n✓ Model downloaded');
} else {
    console.log(`✓ Model ${MODEL_ALIAS} found in cache`);
}

console.log(`Loading model ${MODEL_ALIAS}...`);
await model.load();
console.log('✓ Model loaded');

// ── Start Web Service ───────────────────────────────────────────────────────
manager.startWebService();

const client = manager.createResponsesClient(model.id);
console.log(`✓ Web service started at ${client['baseUrl']}`);
client.settings.temperature = 0.7;
if (!imagePath) {
    client.settings.maxOutputTokens = 512;
}

// ── Run Inference ───────────────────────────────────────────────────────────
if (imagePath) console.log(`\nImage: ${imagePath}`);
console.log(`Prompt: ${textInput}\n`);
console.log('--- Response ---');

if (imagePath) {
    // Vision via Responses API (streaming)
    const image = imagePath.startsWith('http')
        ? { url: imagePath, mediaType: getMimeType(imagePath) }
        : readImageAsDataUri(imagePath);

    const input = [{
        type: 'message',
        role: 'user',
        content: [
            {
                type: 'input_image',
                image_url: image.url,
                media_type: image.mediaType,
                detail: 'auto'
            },
            { type: 'input_text', text: textInput }
        ]
    }];

    await client.createStreaming(input, (event) => {
        if (event.type === 'response.output_text.delta') {
            process.stdout.write(event.delta);
        }
    });
} else {
    // Text-only: use Responses API with streaming
    const input = [{ type: 'message', role: 'user', content: [{ type: 'input_text', text: textInput }] }];
    await client.createStreaming(input, (event) => {
        if (event.type === 'response.output_text.delta') {
            process.stdout.write(event.delta);
        }
    });
}

console.log('');

// ── Cleanup ─────────────────────────────────────────────────────────────────
console.log('Unloading model...');
await model.unload();
console.log('✓ Done');
