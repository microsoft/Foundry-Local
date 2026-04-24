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
// </init>

// <live_transcription>
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

// Create an audio client and live transcription session
const audioClient = speechModel.createAudioClient();
const session = audioClient.createLiveTranscriptionSession();
session.settings.language = 'en';

// <microphone_setup>
// Set up microphone capture with naudiodon2
const audioInput = AudioIO({
    inOptions: {
        channelCount: 1,
        sampleFormat: SampleFormat16Bit,
        sampleRate: 16000,
        deviceId: -1
    }
});
// </microphone_setup>

// Forward microphone audio to the transcription session
audioInput.on('data', async (buffer) => {
    await session.append(new Uint8Array(buffer));
});

// Start the transcription session, then start capturing audio
await session.start();
audioInput.start();
console.log('Listening... Press Ctrl+C to stop.\n');

// Accumulate final transcription text for summarization
let accumulatedText = '';

// Handle graceful shutdown on Ctrl+C using a promise so we can
// await the shutdown and then proceed to summarization.
const stoppedPromise = new Promise((resolve) => {
    let stopping = false;
    process.on('SIGINT', async () => {
        if (stopping) return;
        stopping = true;
        console.log('\nStopping transcription...');
        audioInput.quit();
        await session.stop();
        resolve();
    });
});

// Consume the transcription stream
for await (const segment of session.getTranscriptionStream()) {
    const text = segment.content
        .map((c) => c.text ?? c.transcript)
        .join('');
    if (segment.isFinal) {
        process.stdout.write(`\r${text}\n`);
        accumulatedText += text + ' ';
    } else {
        process.stdout.write(`\r${text}`);
    }
}

// Wait for shutdown to complete
await stoppedPromise;

// Clean up the transcription session and speech model
session.dispose();
await speechModel.unload();
console.log('Speech model unloaded.');
// </live_transcription>

// <summarization>
if (accumulatedText.trim().length === 0) {
    console.log('No transcription captured. Skipping summarization.');
} else {
    console.log('\n--- Summarizing transcription ---\n');

    // Load the chat model for summarization
    const chatModel = await manager.catalog.getModel('qwen2.5-0.5b');
    await chatModel.download((progress) => {
        process.stdout.write(
            `\rDownloading chat model: ${progress.toFixed(2)}%`
        );
    });
    console.log('\nChat model downloaded.');

    await chatModel.load();
    console.log('Chat model loaded.');

    // Summarize the transcription into organized notes
    const chatClient = chatModel.createChatClient();
    const messages = [
        {
            role: 'system',
            content: 'You are a note-taking assistant. Summarize ' +
                     'the following transcription into organized, ' +
                     'concise notes with bullet points.'
        },
        {
            role: 'user',
            content: accumulatedText
        }
    ];

    const response = await chatClient.completeChat(messages);
    const summary = response.choices[0]?.message?.content;
    console.log(`\nSummary:\n${summary}`);

    // Clean up
    await chatModel.unload();
    console.log('\nDone. Models unloaded.');
}
// </summarization>
// </complete_code>
