// <complete_code>
// <imports>
import { FoundryLocalManager } from 'foundry-local-sdk';
import { OpenAI } from 'openai';
// </imports>

// This sample runs the SAME chat prompt two ways against Foundry Local:
//   1. Native in-process inference via the SDK's chat client.
//   2. The local OpenAI-compatible web server (/v1/chat/completions).
const endpointUrl = 'http://localhost:5764';
const prompt = 'Why is the sky blue?';

function section(title) {
    console.log(`\n${'═'.repeat(60)}`);
    console.log(`  ${title}`);
    console.log('═'.repeat(60));
}

// Initialize the Foundry Local SDK
console.log('Initializing Foundry Local SDK...');

// <init>
// `webServiceUrls` is supplied so the local web server starts on a known endpoint.
const manager = FoundryLocalManager.create({
    appName: 'foundry_local_samples',
    logLevel: 'info',
    webServiceUrls: endpointUrl
});
// </init>
console.log('✓ SDK initialized successfully');

// Discover available execution providers and their registration status.
const eps = manager.discoverEps();
const maxNameLen = 30;
console.log('\nAvailable execution providers:');
console.log(`  ${'Name'.padEnd(maxNameLen)}  Registered`);
console.log(`  ${'─'.repeat(maxNameLen)}  ──────────`);
for (const ep of eps) {
    console.log(`  ${ep.name.padEnd(maxNameLen)}  ${ep.isRegistered}`);
}

// Download and register all execution providers with per-EP progress.
// EP packages include dependencies and may be large.
// Download is only required again if a new version of the EP is released.
console.log('\nDownloading execution providers:');
if (eps.length > 0) {
    let currentEp = '';
    await manager.downloadAndRegisterEps((epName, percent) => {
        if (epName !== currentEp) {
            if (currentEp !== '') {
                process.stdout.write('\n');
            }
            currentEp = epName;
        }
        process.stdout.write(`\r  ${epName.padEnd(maxNameLen)}  ${percent.toFixed(1).padStart(5)}%`);
    });
    process.stdout.write('\n');
} else {
    console.log('No execution providers to download.');
}

// <model_setup>
// Get the model object
const modelAlias = 'qwen2.5-0.5b'; // Using an available model from the list above
const model = await manager.catalog.getModel(modelAlias);

// Download the model
console.log(`\nDownloading model ${modelAlias}...`);
await model.download((progress) => {
    process.stdout.write(`\rDownloading... ${progress.toFixed(2)}%`);
});
console.log('\n✓ Model downloaded');

// Load the model
console.log(`\nLoading model ${modelAlias}...`);
await model.load();
console.log('✓ Model loaded');
// </model_setup>

// <native_inference>
section('NATIVE IN-PROCESS INFERENCE');

// Create chat client
console.log('Creating chat client...');
const chatClient = model.createChatClient();
console.log('✓ Chat client created');

// Example chat completion
console.log(`\nPrompt: ${prompt}`);
const completion = await chatClient.completeChat([
    { role: 'user', content: prompt }
]);

console.log('\nResponse:');
console.log(completion.choices[0]?.message?.content);

// Example streaming completion
console.log('\nStreaming a second prompt...');
for await (const chunk of chatClient.completeStreamingChat(
    [{ role: 'user', content: 'Write a short poem about programming.' }]
)) {
    const content = chunk.choices?.[0]?.delta?.content;
    if (content) {
        process.stdout.write(content);
    }
}
console.log('\n');
// </native_inference>

// <web_server>
section('LOCAL WEB SERVER (OpenAI-compatible /v1/chat/completions)');

// Start the web service and call it with the same prompt using the OpenAI client.
console.log('Starting web service...');
manager.startWebService();
console.log('✓ Web service started');

const openai = new OpenAI({
    baseURL: endpointUrl + '/v1',
    apiKey: 'notneeded',
});

console.log(`\nPrompt: ${prompt}`);
const response = await openai.chat.completions.create({
    model: model.id,
    messages: [
        { role: 'user', content: prompt },
    ],
});

console.log('\nResponse:');
console.log(response.choices[0].message.content);
// </web_server>

// <cleanup>
// Unload the model and stop the web service
console.log('\nUnloading model and stopping web service...');
await model.unload();
manager.stopWebService();
console.log('✓ Model unloaded and web service stopped');
// </cleanup>
// </complete_code>
