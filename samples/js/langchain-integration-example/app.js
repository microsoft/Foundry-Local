import { ChatOpenAI } from "@langchain/openai";
import { ChatPromptTemplate } from "@langchain/core/prompts";
import { FoundryLocalManager } from '@prathikrao/foundry-local-sdk';

// Initialize the Foundry Local SDK
console.log('Initializing Foundry Local SDK...');

const endpointUrl = 'http://localhost:5764';

const manager = FoundryLocalManager.create({
    appName: 'foundry_local_samples',
    logLevel: 'info',
    webServiceUrls: endpointUrl
});
console.log('✓ SDK initialized successfully');

// Get the model object
const modelAlias = 'qwen2.5-0.5b'; // Using an available model from the list above
const model = await manager.catalog.getModel(modelAlias);

// Download the model
console.log(`\nDownloading model ${modelAlias}...`);
model.download();
console.log('✓ Model downloaded');

// Load the model
console.log(`\nLoading model ${modelAlias}...`);
model.load();
console.log('✓ Model loaded');

// Start the web service
console.log('\nStarting web service...');
manager.startWebService();
console.log('✓ Web service started');


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

// Tidy up
console.log('Unloading model and stopping web service...');
model.unload();
manager.stopWebService();
console.log(`✓ Model unloaded and web service stopped`);