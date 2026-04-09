// <complete_code>
// <imports>
import { FoundryLocalManager } from 'foundry-local-sdk';
import * as readline from 'readline';
// </imports>

// <tool_definitions>
// --- Tool definitions ---
const tools = [
    {
        type: 'function',
        function: {
            name: 'get_weather',
            description: 'Get the current weather for a location',
            parameters: {
                type: 'object',
                properties: {
                    location: {
                        type: 'string',
                        description: 'The city or location'
                    },
                    unit: {
                        type: 'string',
                        enum: ['celsius', 'fahrenheit'],
                        description: 'Temperature unit'
                    }
                },
                required: ['location']
            }
        }
    },
    {
        type: 'function',
        function: {
            name: 'calculate',
            description: 'Perform a math calculation',
            parameters: {
                type: 'object',
                properties: {
                    expression: {
                        type: 'string',
                        description:
                            'The math expression to evaluate'
                    }
                },
                required: ['expression']
            }
        }
    }
];

// --- Tool implementations ---
function getWeather(location, unit = 'celsius') {
    return {
        location,
        temperature: unit === 'celsius' ? 22 : 72,
        unit,
        condition: 'Sunny'
    };
}

function calculate(expression) {
    // Input is validated against a strict allowlist of numeric/math characters,
    // making this safe from code injection in this tutorial context.
    const allowed = /^[0-9+\-*/(). ]+$/;
    if (!allowed.test(expression)) {
        return { error: 'Invalid expression' };
    }
    try {
        const result = Function(
            `"use strict"; return (${expression})`
        )();
        return { expression, result };
    } catch (err) {
        return { error: err.message };
    }
}

const toolFunctions = {
    get_weather: (args) => getWeather(args.location, args.unit),
    calculate: (args) => calculate(args.expression)
};
// </tool_definitions>

// <tool_loop>
async function processToolCalls(messages, response, chatClient) {
    let choice = response.choices[0]?.message;

    while (choice?.tool_calls?.length > 0) {
        messages.push(choice);

        for (const toolCall of choice.tool_calls) {
            const functionName = toolCall.function.name;
            const args = JSON.parse(toolCall.function.arguments);
            console.log(
                `  Tool call: ${functionName}` +
                `(${JSON.stringify(args)})`
            );

            const result = toolFunctions[functionName](args);
            messages.push({
                role: 'tool',
                tool_call_id: toolCall.id,
                content: JSON.stringify(result)
            });
        }

        response = await chatClient.completeChat(
            messages, { tools }
        );
        choice = response.choices[0]?.message;
    }

    return choice?.content ?? '';
}
// </tool_loop>

// <init>
// --- Main application ---
const manager = FoundryLocalManager.create({
    appName: 'foundry_local_samples',
    logLevel: 'info'
});

// Download and register all execution providers.
await manager.downloadAndRegisterEps();

const model = await manager.catalog.getModel('qwen2.5-0.5b');

await model.download((progress) => {
    process.stdout.write(
        `\rDownloading model: ${progress.toFixed(2)}%`
    );
});
console.log('\nModel downloaded.');

await model.load();
console.log('Model loaded and ready.');

const chatClient = model.createChatClient();

const messages = [
    {
        role: 'system',
        content:
            'You are a helpful assistant with access to tools. ' +
            'Use them when needed to answer questions accurately.'
    }
];

const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout
});

const askQuestion = (prompt) =>
    new Promise((resolve) => rl.question(prompt, resolve));

console.log(
    '\nTool-calling assistant ready! Type \'quit\' to exit.\n'
);

while (true) {
    const userInput = await askQuestion('You: ');
    if (
        userInput.trim().toLowerCase() === 'quit' ||
        userInput.trim().toLowerCase() === 'exit'
    ) {
        break;
    }

    messages.push({ role: 'user', content: userInput });

    const response = await chatClient.completeChat(
        messages, { tools }
    );
    const answer = await processToolCalls(
        messages, response, chatClient
    );

    messages.push({ role: 'assistant', content: answer });
    console.log(`Assistant: ${answer}\n`);
}

await model.unload();
console.log('Model unloaded. Goodbye!');
rl.close();
// </init>
// </complete_code>
