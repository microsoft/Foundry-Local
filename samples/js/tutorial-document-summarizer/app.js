// <complete_code>
// <imports>
import { FoundryLocalManager } from 'foundry-local-sdk';
import { readFileSync, readdirSync, statSync } from 'fs';
import { join, basename } from 'path';
// </imports>

async function summarizeFile(chatClient, filePath, systemPrompt) {
    const content = readFileSync(filePath, 'utf-8');
    const messages = [
        { role: 'system', content: systemPrompt },
        { role: 'user', content: content }
    ];

    const response = await chatClient.completeChat(messages);
    console.log(response.choices[0]?.message?.content);
}

async function summarizeDirectory(chatClient, directory, systemPrompt) {
    const txtFiles = readdirSync(directory)
        .filter(f => f.endsWith('.txt'))
        .sort();

    if (txtFiles.length === 0) {
        console.log(`No .txt files found in ${directory}`);
        return;
    }

    for (const fileName of txtFiles) {
        console.log(`--- ${fileName} ---`);
        await summarizeFile(chatClient, join(directory, fileName), systemPrompt);
        console.log();
    }
}

// <init>
// Initialize the Foundry Local SDK
const manager = FoundryLocalManager.create({
    appName: 'doc-summarizer',
    logLevel: 'info'
});

// Select and load a model from the catalog
const model = await manager.catalog.getModel('qwen2.5-0.5b');

await model.download((progress) => {
    process.stdout.write(`\rDownloading model: ${progress.toFixed(2)}%`);
});
console.log('\nModel downloaded.');

await model.load();
console.log('Model loaded and ready.\n');

// Create a chat client
const chatClient = model.createChatClient();
// </init>

// <summarization>
const systemPrompt =
    'Summarize the following document into concise bullet points. ' +
    'Focus on the key points and main ideas.';

// <file_reading>
const target = process.argv[2] || 'document.txt';
// </file_reading>

try {
    const stats = statSync(target);
    if (stats.isDirectory()) {
        await summarizeDirectory(chatClient, target, systemPrompt);
    } else {
        console.log(`--- ${basename(target)} ---`);
        await summarizeFile(chatClient, target, systemPrompt);
    }
} catch {
    console.log(`--- ${basename(target)} ---`);
    await summarizeFile(chatClient, target, systemPrompt);
}
// </summarization>

// Clean up
await model.unload();
console.log('\nModel unloaded. Done!');
// </complete_code>
