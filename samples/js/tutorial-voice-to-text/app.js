// <complete_code>
// <imports>
import { FoundryLocalManager } from 'foundry-local-sdk';
// </imports>

// <init>
// Initialize the Foundry Local SDK
const manager = FoundryLocalManager.create({
    appName: 'note-taker',
    logLevel: 'info'
});
// </init>

// <transcription>
// Load the speech-to-text model
const speechModel = await manager.catalog.getModel('whisper');
await speechModel.download((progress) => {
    process.stdout.write(
        `\rDownloading speech model: ${progress.toFixed(2)}%`
    );
});
console.log('\nSpeech model downloaded.');

await speechModel.load();
console.log('Speech model loaded.');

// Transcribe the audio file
const audioClient = speechModel.createAudioClient();
const transcription = await audioClient.transcribe(
    './meeting-notes.wav'
);
console.log(`\nTranscription:\n${transcription.text}`);

// Unload the speech model to free memory
await speechModel.unload();
// </transcription>

// <summarization>
// Load the chat model for summarization
const chatModel = await manager.catalog.getModel('phi-3.5-mini');
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
        content: transcription.text
    }
];

const response = await chatClient.completeChat(messages);
const summary = response.choices[0]?.message?.content;
console.log(`\nSummary:\n${summary}`);

// Clean up
await chatModel.unload();
console.log('\nDone. Models unloaded.');
// </summarization>
// </complete_code>
