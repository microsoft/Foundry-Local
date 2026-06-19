// Audio Transcription Example — Foundry Local JS SDK
//
// Two modes in a single app:
//   • Live microphone streaming (default) using Nemotron streaming ASR.
//   • File-based transcription via `--file <path>` using Whisper (defaults to
//     the bundled Recording.mp3 when no path is given).
//
// Live capture requires: npm install naudiodon2
//
// Usage:
//   node app.js                 # live mic streaming (Nemotron)
//   node app.js --file          # transcribe bundled Recording.mp3 (Whisper)
//   node app.js --file <path>   # transcribe a specific audio file (Whisper)

import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { FoundryLocalManager } from 'foundry-local-sdk';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

// --- Parse CLI args ---
// `--file` (optionally followed by a path) selects file-based transcription.
// Without a path it falls back to the bundled Recording.mp3.
const fileFlagIndex = process.argv.indexOf('--file');
const fileMode = fileFlagIndex !== -1;
const fileArg = fileMode ? process.argv[fileFlagIndex + 1] : undefined;
const audioFilePath = fileMode
    ? (fileArg && !fileArg.startsWith('--') ? fileArg : path.join(__dirname, 'Recording.mp3'))
    : undefined;

console.log('╔══════════════════════════════════════════════════════════╗');
console.log('║   Foundry Local — Audio Transcription (JS SDK)          ║');
console.log('╚══════════════════════════════════════════════════════════╝');
console.log();

// Initialize the Foundry Local SDK
console.log('Initializing Foundry Local SDK...');
const manager = FoundryLocalManager.create({
    appName: 'foundry_local_samples',
    logLevel: 'info'
});
console.log('✓ SDK initialized');

if (fileMode) {
    await runFileTranscription(manager, audioFilePath);
} else {
    await runLiveTranscription(manager);
}

// --- File-based transcription (Whisper) ---
async function runFileTranscription(manager, audioFile) {
    console.log(`\nMode: file-based transcription (${audioFile})`);

    // Download and register all execution providers.
    let currentEp = '';
    await manager.downloadAndRegisterEps((epName, percent) => {
        if (epName !== currentEp) {
            if (currentEp !== '') process.stdout.write('\n');
            currentEp = epName;
        }
        process.stdout.write(`\r  ${epName.padEnd(30)}  ${percent.toFixed(1).padStart(5)}%`);
    });
    if (currentEp !== '') process.stdout.write('\n');

    // Get the Whisper model
    const modelAlias = 'whisper-tiny';
    const model = await manager.catalog.getModel(modelAlias);
    console.log(`Using model: ${model.id}`);

    console.log(`\nDownloading model ${modelAlias}...`);
    await model.download((progress) => {
        process.stdout.write(`\rDownloading... ${progress.toFixed(2)}%`);
    });
    console.log('\n✓ Model downloaded');

    console.log(`\nLoading model ${modelAlias}...`);
    await model.load();
    console.log('✓ Model loaded');

    // Create audio client
    console.log('\nCreating audio client...');
    const audioClient = model.createAudioClient();
    console.log('✓ Audio client created');

    // Non-streaming transcription
    console.log(`\nTranscribing ${audioFile}...`);
    const transcription = await audioClient.transcribe(audioFile);
    console.log('\nAudio transcription result:');
    console.log(transcription.text);
    console.log('✓ Audio transcription completed');

    // Streaming transcription using async iteration
    console.log('\nTesting streaming audio transcription...');
    for await (const result of audioClient.transcribeStreaming(audioFile)) {
        // Output intermediate transcription results as they arrive (no line ending).
        process.stdout.write(result.text);
    }
    console.log('\n✓ Streaming transcription completed');

    // Unload the model
    console.log('\nUnloading model...');
    await model.unload();
    console.log('✓ Model unloaded');
}

// --- Live microphone transcription (Nemotron streaming ASR) ---
async function runLiveTranscription(manager) {
    console.log('\nMode: live microphone streaming');

    // Get and load the nemotron model
    // English-only:
    const modelAlias = 'nemotron-speech-streaming-en-0.6b';
    // Multi-lingual (supports 30+ languages including auto-detect):
    // const modelAlias = 'nemotron-3.5-asr-streaming-0.6b';
    const model = await manager.catalog.getModel(modelAlias);
    if (!model) {
        console.error(`ERROR: Model "${modelAlias}" not found in catalog.`);
        process.exit(1);
    }

    console.log(`Found model: ${model.id}`);
    console.log('Downloading model (if needed)...');
    await model.download((progress) => {
        process.stdout.write(`\rDownloading... ${progress.toFixed(2)}%`);
    });
    console.log('\n✓ Model downloaded');

    console.log('Loading model...');
    await model.load();
    console.log('✓ Model loaded');

    // Create live transcription session (same pattern as C# sample).
    const audioClient = model.createAudioClient();
    const session = audioClient.createLiveTranscriptionSession();

    session.settings.sampleRate = 16000;  // Default is 16000; shown here for clarity
    session.settings.channels = 1;
    session.settings.bitsPerSample = 16;
    session.settings.language = 'en';                  // English (default)
    // Multi-lingual examples:
    // session.settings.language = 'de';     // German
    // session.settings.language = 'zh-CN';  // Chinese (Simplified)
    // session.settings.language = 'auto';   // Auto-detect language

    console.log('Starting streaming session...');
    await session.start();
    console.log('✓ Session started');

    // Read transcription results in background
    const readPromise = (async () => {
        try {
            for await (const result of session.getStream()) {
                const text = result.content?.[0]?.text;
                if (!text) continue;

                // `is_final` is a transcript-state marker only. It should not stop the app.
                if (result.is_final) {
                    process.stdout.write(`\n  [FINAL] ${text}\n`);
                } else {
                    process.stdout.write(text);
                }
            }
        } catch (err) {
            if (err.name !== 'AbortError') {
                console.error('Stream error:', err.message);
            }
        }
    })();

    // --- Microphone capture ---
    // This example uses naudiodon2 for cross-platform audio capture.
    // Install with: npm install naudiodon2
    //
    // If you prefer a different audio library, just push PCM bytes
    // (16-bit signed LE, mono, 16kHz) via session.append().

    let audioInput;
    try {
        const { default: portAudio } = await import('naudiodon2');

        audioInput = portAudio.AudioIO({
            inOptions: {
                channelCount: session.settings.channels,
                sampleFormat: session.settings.bitsPerSample === 16
                    ? portAudio.SampleFormat16Bit
                    : portAudio.SampleFormat32Bit,
                sampleRate: session.settings.sampleRate,
                // Larger chunk size lowers callback frequency and reduces overflow risk.
                framesPerBuffer: 3200,
                // Allow deeper native queue during occasional event-loop stalls.
                maxQueue: 64
            }
        });

        const appendQueue = [];
        let pumping = false;
        let warnedQueueDrop = false;

        const pumpAudio = async () => {
            if (pumping) return;
            pumping = true;
            try {
                while (appendQueue.length > 0) {
                    const pcm = appendQueue.shift();
                    await session.append(pcm);
                }
            } catch (err) {
                console.error('append error:', err.message);
            } finally {
                pumping = false;
                // Handle race where new data arrived after loop exit.
                if (appendQueue.length > 0) {
                    void pumpAudio();
                }
            }
        };

        audioInput.on('data', (buffer) => {
            // Single copy: slice the underlying ArrayBuffer to get an independent Uint8Array.
            const copy = new Uint8Array(buffer.buffer, buffer.byteOffset, buffer.byteLength).slice();

            // Keep a bounded queue to avoid unbounded memory growth.
            if (appendQueue.length >= 100) {
                appendQueue.shift();
                if (!warnedQueueDrop) {
                    warnedQueueDrop = true;
                    console.warn('Audio append queue overflow; dropping oldest chunk to keep stream alive.');
                }
            }

            appendQueue.push(copy);
            void pumpAudio();
        });

        console.log();
        console.log('════════════════════════════════════════════════════════════');
        console.log('  LIVE TRANSCRIPTION ACTIVE');
        console.log('  Speak into your microphone.');
        console.log('  Press Ctrl+C to stop.');
        console.log('════════════════════════════════════════════════════════════');
        console.log();

        audioInput.start();
    } catch (err) {
        console.warn('⚠ Could not initialize microphone (naudiodon2 may not be installed).');
        console.warn('  Install with: npm install naudiodon2');
        console.warn('  Or transcribe a file instead: node app.js --file');
        console.warn('  Falling back to synthetic audio test...');
        console.warn();

        // Fallback: push 2 seconds of synthetic PCM (440Hz sine wave)
        const sampleRate = session.settings.sampleRate;
        const duration = 2;
        const totalSamples = sampleRate * duration;
        const pcmBytes = new Uint8Array(totalSamples * 2);
        for (let i = 0; i < totalSamples; i++) {
            const t = i / sampleRate;
            const sample = Math.round(32767 * 0.5 * Math.sin(2 * Math.PI * 440 * t));
            pcmBytes[i * 2] = sample & 0xFF;
            pcmBytes[i * 2 + 1] = (sample >> 8) & 0xFF;
        }

        // Push in 100ms chunks
        const chunkSize = (sampleRate / 10) * 2;
        for (let offset = 0; offset < pcmBytes.length; offset += chunkSize) {
            const len = Math.min(chunkSize, pcmBytes.length - offset);
            await session.append(pcmBytes.slice(offset, offset + len));
        }

        console.log('✓ Synthetic audio pushed');
        console.log('Waiting briefly for final transcription results...');
        await new Promise((resolve) => setTimeout(resolve, 3000));
        await session.stop();
        await readPromise;
        await model.unload();
        console.log('✓ Done');
        process.exit(0);
    }

    // Handle graceful shutdown
    process.on('SIGINT', async () => {
        console.log('\n\nStopping...');
        if (audioInput) {
            audioInput.quit();
        }
        await session.stop();
        await readPromise;
        await model.unload();
        console.log('✓ Done');
        process.exit(0);
    });
}
