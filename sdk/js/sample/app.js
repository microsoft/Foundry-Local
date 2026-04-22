import { FoundryLocalManager, getOutputText } from 'foundry-local-sdk';
import { readFileSync, writeFileSync, unlinkSync } from 'fs';
import { resolve, extname, join } from 'path';
import { execSync } from 'child_process';
import { tmpdir } from 'os';

// ── Configuration ──────────────────────────────────────────────────────────
const MODEL_ALIAS = 'qwen3.5-4b';
const MAX_IMAGE_DIM = 480;
const MIME_TYPES = { jpg: 'image/jpeg', jpeg: 'image/jpeg', png: 'image/png', gif: 'image/gif', webp: 'image/webp', bmp: 'image/bmp' };

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

// ── Helpers ────────────────────────────────────────────────────────────────

function getMimeType(filePath) {
    return MIME_TYPES[extname(filePath).toLowerCase().replace('.', '')] || 'image/png';
}

function resizeImage(absPath) {
    const tempImg = join(tmpdir(), `fl_resized_${Date.now()}.png`);
    const tempScript = join(tmpdir(), `fl_resize_${Date.now()}.ps1`);
    try {
        writeFileSync(tempScript, [
            'Add-Type -AssemblyName System.Drawing',
            `$img = [System.Drawing.Image]::FromFile("${absPath}")`,
            `$max = ${MAX_IMAGE_DIM}`,
            'if ($img.Width -gt $max -or $img.Height -gt $max) {',
            '    $scale = [Math]::Min($max / $img.Width, $max / $img.Height)',
            '    $nw = [int]($img.Width * $scale); $nh = [int]($img.Height * $scale)',
            '    $bmp = New-Object System.Drawing.Bitmap($nw, $nh)',
            '    $g = [System.Drawing.Graphics]::FromImage($bmp)',
            '    $g.InterpolationMode = "HighQualityBicubic"',
            '    $g.DrawImage($img, 0, 0, $nw, $nh)',
            '    $g.Dispose(); $img.Dispose()',
            `    $bmp.Save("${tempImg}", [System.Drawing.Imaging.ImageFormat]::Png)`,
            '    $bmp.Dispose()',
            '    Write-Host "resized:${nw}x${nh}"',
            '} else { $img.Dispose(); Write-Host "ok" }',
        ].join('\n'));
        const out = execSync(`powershell -NoProfile -ExecutionPolicy Bypass -File "${tempScript}"`, { encoding: 'utf8' }).trim();
        unlinkSync(tempScript);
        if (out.startsWith('resized:')) {
            const base64 = readFileSync(tempImg).toString('base64');
            unlinkSync(tempImg);
            return `data:image/png;base64,${base64}`;
        }
    } catch {
        try { unlinkSync(tempScript); } catch {}
        try { unlinkSync(tempImg); } catch {}
    }
    return null;
}

function readImageAsDataUri(filePath) {
    const absPath = resolve(filePath);
    const resized = resizeImage(absPath);
    if (resized) {
        console.log(`  Resized image: ${(resized.length * 0.75 / 1024).toFixed(0)} KB`);
        return { url: resized, mediaType: 'image/png' };
    }
    const mime = getMimeType(absPath);
    const b64 = readFileSync(absPath).toString('base64');
    const url = `data:${mime};base64,${b64}`;
    console.log(`  Image (no resize needed): ${(b64.length * 0.75 / 1024).toFixed(0)} KB`);
    return { url, mediaType: mime };
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
    let image;
    if (imagePath.startsWith('http')) {
        console.log('Downloading image...');
        const res = await fetch(imagePath);
        if (!res.ok) { console.error(`Failed to fetch image: HTTP ${res.status}`); process.exit(1); }
        const contentType = res.headers.get('content-type')?.split(';')[0].trim() || 'image/jpeg';
        const buf = Buffer.from(await res.arrayBuffer());
        // Save to temp file so we can resize it (large images cause OGA errors)
        const ext = contentType.split('/')[1] || 'jpg';
        const tempFile = join(tmpdir(), `fl_download_${Date.now()}.${ext}`);
        writeFileSync(tempFile, buf);
        try {
            image = readImageAsDataUri(tempFile);
        } finally {
            try { unlinkSync(tempFile); } catch {}
        }
    } else {
        image = readImageAsDataUri(imagePath);
    }
    input[0].content.push({ type: 'input_image', image_url: image.url, media_type: image.mediaType, detail: 'auto' });
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
