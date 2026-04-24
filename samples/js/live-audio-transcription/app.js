// <complete_code>
// <imports>
import { FoundryLocalManager } from 'foundry-local-sdk';
import { AudioIO, SampleFormat16Bit } from 'naudiodon2';
// </imports>

// <init>
// Initialize the Foundry Local SDK
const manager = FoundryLocalManager.create({
    appName: 'foundry_local_samples',
    logLevel: 'info'
});

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

// Download and load the speech-to-text model
const speechModel = await manager.catalog.getModel('nemotron-speech-streaming-en-0.6b');
await speechModel.download((progress) => {
    process.stdout.write(
        `\rDownloading speech model: ${progress.toFixed(2)}%`
    );
});
console.log('\nSpeech model downloaded.');

await speechModel.load();
console.log('Speech model loaded.');
// </init>

// <live_transcription>
// Create an audio client and live transcription session
const audioClient = speechModel.createAudioClient();
const session = audioClient.createLiveTranscriptionSession();
session.settings.language = 'en';

// Set up microphone capture with naudiodon2
const audioInput = AudioIO({
    inOptions: {
        channelCount: 1,
        sampleFormat: SampleFormat16Bit,
        sampleRate: 16000,
        deviceId: -1
    }
});

// Forward microphone audio to the transcription session
audioInput.on('data', async (buffer) => {
    await session.append(new Uint8Array(buffer));
});

// Start the transcription session, then start capturing audio
await session.start();
audioInput.start();
console.log('Listening... Press Ctrl+C to stop.\n');

// Handle graceful shutdown on Ctrl+C
let stopping = false;
process.on('SIGINT', async () => {
    if (stopping) return;
    stopping = true;
    console.log('\nStopping...');
    audioInput.quit();
    await session.stop();
});

// Consume the transcription stream
for await (const segment of session.getTranscriptionStream()) {
    const text = segment.content
        .map((c) => c.text ?? c.transcript)
        .join('');
    if (segment.isFinal) {
        process.stdout.write(`\r${text}\n`);
    } else {
        process.stdout.write(`\r${text}`);
    }
}

// Clean up
session.dispose();
await speechModel.unload();
console.log('Done. Model unloaded.');
// </live_transcription>
// </complete_code>
