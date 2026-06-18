// <complete_code>
// <imports>
import { FoundryLocalManager } from 'foundry-local-sdk';
import { OpenAI } from 'openai';
// </imports>

function getResponseText(response) {
    if (typeof response.output_text === 'string') {
        return response.output_text;
    }
    return (response.output ?? [])
        .flatMap((item) => Array.isArray(item.content) ? item.content : [])
        .filter((part) => part.type === 'output_text' && typeof part.text === 'string')
        .map((part) => part.text)
        .join('');
}

// <init>
const endpointUrl = 'http://localhost:5764';

console.log('Initializing Foundry Local SDK...');
const manager = FoundryLocalManager.create({
    appName: 'foundry_local_samples',
    logLevel: 'info',
    webServiceUrls: endpointUrl,
});
console.log('SDK initialized successfully');

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

// <model_setup>
const modelAlias = 'qwen2.5-0.5b';
const model = await manager.catalog.getModel(modelAlias);

console.log(`\nDownloading model ${modelAlias}...`);
await model.download((progress) => {
    process.stdout.write(`\rDownloading... ${progress.toFixed(2)}%`);
});
console.log('\nModel downloaded');

console.log('\nLoading model...');
await model.load();
console.log('Model loaded');
// </model_setup>

// <server_setup>
console.log('\nStarting web service...');
manager.startWebService();
console.log('Web service started');

// <<<<<< OPENAI SDK USAGE >>>>>>
// Use the OpenAI SDK to call the local Foundry web service Responses API
const openai = new OpenAI({
    baseURL: endpointUrl + '/v1',
    apiKey: 'notneeded',
});
// </server_setup>

try {
    console.log('\nTesting a non-streaming Responses call...');
    const response = await openai.responses.create({
        model: model.id,
        input: 'Reply with one short sentence about local AI.',
    });
    console.log(`[ASSISTANT]: ${getResponseText(response)}`);

    console.log('\nTesting a streaming Responses call...');
    const stream = await openai.responses.create({
        model: model.id,
        input: 'Count from one to three.',
        stream: true,
    });

    process.stdout.write('[ASSISTANT STREAM]: ');
    for await (const event of stream) {
        if (event.type === 'response.output_text.delta') {
            process.stdout.write(event.delta);
        }
    }
    process.stdout.write('\n');

    console.log('\nTesting Responses tool calling...');
    const tools = [
        {
            type: 'function',
            name: 'get_weather',
            description: 'Get the current weather. This sample always returns Seattle weather.',
            parameters: {
                type: 'object',
                properties: {},
                additionalProperties: false,
            },
        },
    ];

    const toolResponse = await openai.responses.create({
        model: model.id,
        input: 'Use the get_weather tool and then answer with the weather.',
        tools,
        tool_choice: 'required',
        store: true,
    });

    const functionCall = toolResponse.output?.find((item) => item.type === 'function_call');
    if (!functionCall) {
        throw new Error('Expected the model to call get_weather.');
    }

    console.log(`[TOOL CALL]: ${functionCall.name}(${functionCall.arguments})`);

    const finalResponse = await openai.responses.create({
        model: model.id,
        previous_response_id: toolResponse.id,
        input: [
            {
                type: 'function_call_output',
                call_id: functionCall.call_id,
                output: JSON.stringify({ location: 'Seattle', weather: '72 degrees F and sunny' }),
            },
        ],
        tools,
    });

    console.log(`[ASSISTANT FINAL]: ${getResponseText(finalResponse)}`);
    // <<<<<< END OPENAI SDK USAGE >>>>>>
} finally {
    // Tidy up
    manager.stopWebService();
    await model.unload();
}
// </complete_code>
