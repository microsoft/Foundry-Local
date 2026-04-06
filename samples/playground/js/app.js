import { FoundryLocalManager } from 'foundry-local-sdk-winml';
import readline from 'readline';
import path from 'path';
import { section, showEpTable, showCatalog, createDownloadBar,
         printUserMsg, createStreamBox, askUser } from './ui.js';

// ── Initialize ───────────────────────────────────────────────────────────

const manager = FoundryLocalManager.create({
    appName: 'foundry-local-playground',
    logLevel: 'info',
});

// ── Discover & download execution providers ──────────────────────────────

section('Execution Providers');

const eps = manager.discoverEps();
const epUi = showEpTable(eps);

const unregistered = eps.filter(ep => !ep.isRegistered);
if (unregistered.length > 0) {
    const result = await manager.downloadAndRegisterEps(
        unregistered.map(ep => ep.name),
        epUi.onProgress,
    );
    epUi.finalize(result);
}

// ── Browse model catalog & pick a model ──────────────────────────────────

section('Model Catalog');

const models = await manager.catalog.getModels();
models.sort((a, b) => {
    if (a.isCached !== b.isCached) return a.isCached ? -1 : 1;
    return a.alias.localeCompare(b.alias);
});
for (const m of models) {
    m.variants.sort((a, b) => (a.isCached === b.isCached ? 0 : a.isCached ? -1 : 1));
}

const rows = showCatalog(models);

const rl = readline.createInterface({ input: process.stdin, output: process.stdout });
const choice = await askUser(rl, `\n  Select a model [\x1b[36m1-${rows.length}\x1b[0m]: `);
const selectedIdx = parseInt(choice, 10) - 1;

if (isNaN(selectedIdx) || selectedIdx < 0 || selectedIdx >= rows.length) {
    console.log('  Invalid selection.');
    rl.close();
    process.exit(1);
}

const chosen = rows[selectedIdx];
const modelAlias = chosen.model.alias;
console.log(`\n  Selected: \x1b[32m${modelAlias}\x1b[0m (${chosen.variant.id})`);

// ── Download & load the model ────────────────────────────────────────────

const model = await manager.catalog.getModel(modelAlias);
model.selectVariant(chosen.variant);
section(`Model – ${modelAlias}`);

if (!model.isCached) {
    const dl = createDownloadBar(modelAlias);
    await model.download(dl.onProgress);
    dl.finalize();
}

await model.load();
console.log('  \x1b[32m✓\x1b[0m Model loaded\n');

// ── Detect task type ─────────────────────────────────────────────────────

const task = (chosen.model.info?.task || '').toLowerCase();
const isAudio = task.includes('speech-recognition') ||
    task.includes('speech-to-text') ||
    modelAlias.toLowerCase().includes('whisper');

if (isAudio) {
    // ── Audio Transcription ──────────────────────────────────────────────

    section('Audio Transcription  (enter a file path, /quit to exit)');

    const audioClient = model.createAudioClient();

    while (true) {
        const input = await askUser(rl, '  \x1b[36maudio file> \x1b[0m');
        const trimmed = input.trim();
        if (!trimmed) continue;
        if (['/quit', '/exit', '/q'].includes(trimmed)) break;

        const audioPath = path.resolve(trimmed);
        console.log(`  ${audioPath}\n`);

        const box = createStreamBox();
        try {
            for await (const result of audioClient.transcribeStreaming(audioPath)) {
                if (result.text) {
                    for (const char of result.text) box.write(char);
                }
            }
        } catch (err) {
            box.finish();
            console.log(`  \x1b[31mError: ${err.message}\x1b[0m\n`);
            continue;
        }
        box.finish();
    }
} else {
    // ── Interactive Chat ─────────────────────────────────────────────────

    section('Chat  (type a message, /quit to exit)');

    const chatClient = model.createChatClient();
    const messages = [{ role: 'system', content: 'You are a helpful assistant.' }];

    while (true) {
        const input = await askUser(rl);
        const trimmed = input.trim();
        if (!trimmed) continue;
        if (['/quit', '/exit', '/q'].includes(trimmed)) break;

        process.stdout.write('\x1b[1A\r\x1b[K');
        printUserMsg(trimmed);

        messages.push({ role: 'user', content: trimmed });

        const box = createStreamBox();
        let response = '';

        for await (const chunk of chatClient.completeStreamingChat(messages)) {
            const content = chunk.choices?.[0]?.message?.content;
            if (content) {
                response += content;
                for (const char of content) box.write(char);
            }
        }
        box.finish();

        messages.push({ role: 'assistant', content: response });
    }
}

// ── Clean up ─────────────────────────────────────────────────────────────

rl.close();
process.stdin.unref();
await model.unload();
    