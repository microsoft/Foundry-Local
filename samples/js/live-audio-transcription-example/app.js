// Live Audio Transcription Example — Foundry Local JS SDK
//
// Demonstrates real-time microphone-to-text using the JS SDK.
// Requires: npm install foundry-local-sdk naudiodon2
//
// Usage: node app.js

import { FoundryLocalManager } from 'foundry-local-sdk';

console.log('╔══════════════════════════════════════════════════════════╗');
console.log('║   Foundry Local — Live Audio Transcription (JS SDK)     ║');
console.log('╚══════════════════════════════════════════════════════════╝');
console.log();

// Initialize the Foundry Local SDK
console.log('Initializing Foundry Local SDK...');
const manager = FoundryLocalManager.create({
    appName: 'foundry_local_live_audio',
    logLevel: 'info'
});
console.log('✓ SDK initialized');

// Get and load the nemotron model
const modelAlias = 'nemotron';
let model = await manager.catalog.getModel(modelAlias);
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

// Create live transcription session
const client = model.createLiveTranscriptionClient();
client.settings.sampleRate = 16000;  // Default is 16000; shown here for clarity
client.settings.channels = 1;
client.settings.bitsPerSample = 16;
client.settings.language = 'en';

console.log('Starting streaming session...');
await client.start();
console.log('✓ Session started');

// Read transcription results in background
const readPromise = (async () => {
    try {
        for await (const result of client.getTranscriptionStream()) {
            const text = result.content?.[0]?.text;
            if (result.is_final) {
                console.log();
                console.log(`  [FINAL] ${text}`);
            } else if (text) {
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
// (16-bit signed LE, mono, 16kHz) via client.pushAudioData().

let audioInput;
try {
    const { default: portAudio } = await import('naudiodon2');

    audioInput = portAudio.AudioIO({
        inOptions: {
            channelCount: 1,
            sampleFormat: portAudio.SampleFormatFloat32,  // Will need conversion to Int16
            sampleRate: 16000,
            framesPerBuffer: 1600  // 100ms chunks
        }
    });

    audioInput.on('data', (buffer) => {
        // Convert Float32 to Int16 PCM
        const float32 = new Float32Array(buffer.buffer, buffer.byteOffset, buffer.length / 4);
        const int16 = new Int16Array(float32.length);
        for (let i = 0; i < float32.length; i++) {
            int16[i] = Math.max(-32768, Math.min(32767, Math.round(float32[i] * 32767)));
        }
        const pcmBytes = new Uint8Array(int16.buffer);
        client.pushAudioData(pcmBytes).catch(() => {});
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
    console.warn('  Falling back to synthetic audio test...');
    console.warn();

    // Fallback: push 2 seconds of synthetic PCM (440Hz sine wave)
    const sampleRate = 16000;
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
        await client.pushAudioData(pcmBytes.slice(offset, offset + len));
    }

    console.log('✓ Synthetic audio pushed');
}

// Handle graceful shutdown
process.on('SIGINT', async () => {
    console.log('\n\nStopping...');
    if (audioInput) {
        audioInput.quit();
    }
    await client.stop();
    await readPromise;
    await model.unload();
    console.log('✓ Done');
    process.exit(0);
});
