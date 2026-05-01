// <complete_code>
// <imports>
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
const endpointUrl = process.env.FOUNDRY_LOCAL_ENDPOINT ?? 'http://127.0.0.1:52495';
const modelId = process.env.FOUNDRY_LOCAL_MODEL ?? 'qwen2.5-0.5b';
// </init>

// Start the Foundry Local web service separately, for example with the
// C# foundry-local-web-server sample, then point this sample at that URL.
// <server_setup>
const openai = new OpenAI({
    baseURL: endpointUrl + '/v1',
    apiKey: 'notneeded',
});
// </server_setup>

// <<<<<< OPENAI RESPONSES SDK USAGE >>>>>>
console.log(`Using Foundry Local web service at ${endpointUrl}`);
console.log(`Using model ${modelId}`);

console.log('\nTesting a non-streaming Responses call...');
const response = await openai.responses.create({
    model: modelId,
    input: 'Reply with one short sentence about local AI.',
});
console.log(`[ASSISTANT]: ${getResponseText(response)}`);

console.log('\nTesting a streaming Responses call...');
const stream = await openai.responses.create({
    model: modelId,
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
    model: modelId,
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
    model: modelId,
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
// <<<<<< END OPENAI RESPONSES SDK USAGE >>>>>>
// </complete_code>
