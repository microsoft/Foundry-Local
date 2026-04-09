// <complete_code>
// <imports>
import { FoundryLocalManager } from 'foundry-local-sdk';
import * as readline from 'readline';
// </imports>

// <init>
// Initialize the Foundry Local SDK
const manager = FoundryLocalManager.create({
    appName: 'foundry_local_samples',
    logLevel: 'info'
});

// Select and load a model from the catalog
const model = await manager.catalog.getModel('qwen2.5-0.5b');

await model.download((progress) => {
    process.stdout.write(`\rDownloading model: ${progress.toFixed(2)}%`);
});
console.log('\nModel downloaded.');

await model.load();
console.log('Model loaded and ready.');

// Create a chat client
const chatClient = model.createChatClient();
// </init>

// <system_prompt>
// Start the conversation with a system prompt
const messages = [
    {
        role: 'system',
        content: 'You are a helpful, friendly assistant. Keep your responses ' +
                 'concise and conversational. If you don\'t know something, say so.'
    }
];
// </system_prompt>

// Set up readline for console input
const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout
});

const askQuestion = (prompt) => new Promise((resolve) => rl.question(prompt, resolve));

console.log('\nChat assistant ready! Type \'quit\' to exit.\n');

// <conversation_loop>
while (true) {
    const userInput = await askQuestion('You: ');
    if (userInput.trim().toLowerCase() === 'quit' ||
        userInput.trim().toLowerCase() === 'exit') {
        break;
    }

    // Add the user's message to conversation history
    messages.push({ role: 'user', content: userInput });

    // <streaming>
    // Stream the response token by token
    process.stdout.write('Assistant: ');
    let fullResponse = '';
    for await (const chunk of chatClient.completeStreamingChat(messages)) {
        const content = chunk.choices?.[0]?.delta?.content;
        if (content) {
            process.stdout.write(content);
            fullResponse += content;
        }
    }
    console.log('\n');
    // </streaming>

    // Add the complete response to conversation history
    messages.push({ role: 'assistant', content: fullResponse });
}
// </conversation_loop>

// Clean up - unload the model
await model.unload();
console.log('Model unloaded. Goodbye!');
rl.close();
// </complete_code>
