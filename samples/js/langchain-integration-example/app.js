// <complete_code>
// <imports>
import { ChatOpenAI } from "@langchain/openai";
import { ChatPromptTemplate } from "@langchain/core/prompts";
import { FoundryLocalManager } from 'foundry-local-sdk';
// </imports>

// Initialize the Foundry Local SDK
console.log('Initializing Foundry Local SDK...');

const endpointUrl = 'http://localhost:5764';

// <init>
const manager = FoundryLocalManager.create({
    appName: 'foundry_local_samples',
    logLevel: 'info',
    webServiceUrls: endpointUrl
});
// </init>
console.log('✓ SDK initialized successfully');

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

// Start the web service
console.log('\nStarting web service...');
manager.startWebService();
console.log('✓ Web service started');

// <langchain_setup>

// Configure ChatOpenAI to use your locally-running model
const llm = new ChatOpenAI({
    model: model.id,
    configuration: {
        baseURL: endpointUrl + '/v1',
        apiKey: 'notneeded'
    },
    temperature: 0.6,
    streaming: false
});

// Create a translation prompt template
const prompt = ChatPromptTemplate.fromMessages([
    {
        role: "system",
        content: "You are a helpful assistant that translates {input_language} to {output_language}."
    },
    {
        role: "user",
        content: "{input}"
    }
]);

// Build a simple chain by connecting the prompt to the language model
const chain = prompt.pipe(llm);
// </langchain_setup>

// <chat_completion>
const input = "I love to code.";
console.log(`Translating '${input}' to French...`);

// Run the chain with your inputs
await chain.invoke({
    input_language: "English",
    output_language: "French",
    input: input
}).then(aiMsg => {
    // Print the result content
    console.log(`Response: ${aiMsg.content}`);
}).catch(err => {
    console.error("Error:", err);
});
// </chat_completion>

// Tidy up
console.log('Unloading model and stopping web service...');
await model.unload();
manager.stopWebService();
console.log(`✓ Model unloaded and web service stopped`);
// </complete_code>