// Foundry Local + Learn MCP Server: Local AI Doc Assistant
// Uses Foundry Local for on-device inference and Learn MCP Server for doc retrieval.

import { FoundryLocalManager } from 'foundry-local-sdk';
import * as readline from 'readline';

// --- MCP endpoint ---
const MCP_ENDPOINT = 'https://learn.microsoft.com/api/mcp';

// --- Tool definitions (OpenAI function-calling schema) ---
const tools = [
    {
        type: 'function',
        function: {
            name: 'search_docs',
            description: 'Search Microsoft Learn documentation for a given query. Returns relevant documentation content with titles and URLs. Use this tool whenever the user asks about a Microsoft product, service, SDK, API, or technology.',
            parameters: {
                type: 'object',
                properties: {
                    query: {
                        type: 'string',
                        description: 'The search query about a Microsoft product or technology'
                    }
                },
                required: ['query']
            }
        }
    }
];

// --- Tool implementation: call Learn MCP Server ---
async function searchDocs(query) {
    console.log(`  [Searching Learn MCP Server for: "${query}"]`);

    // MCP uses JSON-RPC over streamable HTTP
    const response = await fetch(MCP_ENDPOINT, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            'Accept': 'application/json, text/event-stream'
        },
        body: JSON.stringify({
            jsonrpc: '2.0',
            id: 1,
            method: 'tools/call',
            params: {
                name: 'microsoft_docs_search',
                arguments: { query }
            }
        })
    });

    if (!response.ok) {
        return { error: `MCP request failed: ${response.status} ${response.statusText}` };
    }

    const contentType = response.headers.get('content-type') || '';

    // Handle SSE/streaming response
    if (contentType.includes('text/event-stream')) {
        const text = await response.text();
        const lines = text.split('\n');
        for (const line of lines) {
            if (line.startsWith('data: ')) {
                try {
                    const data = JSON.parse(line.slice(6));
                    if (data.result) {
                        return formatSearchResults(data.result);
                    }
                } catch { /* skip non-JSON lines */ }
            }
        }
        return { error: 'No result found in SSE response' };
    }

    // Handle direct JSON response
    const data = await response.json();
    if (data.result) {
        return formatSearchResults(data.result);
    }
    return { error: 'Unexpected response format', raw: JSON.stringify(data).slice(0, 500) };
}

function formatSearchResults(result) {
    // MCP tool results come as content arrays
    const content = result.content || [];
    const results = [];

    for (const item of content) {
        if (item.type === 'text') {
            // The text may be a JSON string containing search results
            try {
                const parsed = JSON.parse(item.text);
                if (parsed.results && Array.isArray(parsed.results)) {
                    for (const r of parsed.results.slice(0, 3)) {
                        let entry = `## ${r.title}`;
                        if (r.contentUrl) entry += `\nSource: ${r.contentUrl}`;
                        entry += `\n${r.content}`;
                        results.push(entry);
                    }
                    continue;
                }
            } catch { /* not JSON, use as-is */ }
            results.push(item.text);
        }
    }

    if (results.length === 0) {
        return { message: 'No documentation found for this query.' };
    }

    // Truncate to ~2000 chars to fit in model context window
    let combined = results.join('\n\n---\n\n');
    if (combined.length > 2000) {
        combined = combined.slice(0, 2000) + '\n\n[Truncated]';
    }

    return {
        documentation: combined,
        source: 'Microsoft Learn (learn.microsoft.com)'
    };
}

const toolFunctions = {
    search_docs: async (args) => searchDocs(args.query)
};

// --- Tool-calling loop ---
async function processToolCalls(messages, response, chatClient) {
    let choice = response.choices[0]?.message;

    while (choice?.tool_calls?.length > 0) {
        messages.push(choice);

        for (const toolCall of choice.tool_calls) {
            const functionName = toolCall.function.name;
            const args = JSON.parse(toolCall.function.arguments);
            console.log(`  Tool call: ${functionName}(${JSON.stringify(args)})`);

            const fn = toolFunctions[functionName];
            if (!fn) {
                messages.push({
                    role: 'tool',
                    tool_call_id: toolCall.id,
                    content: JSON.stringify({ error: `Unknown tool: ${functionName}` })
                });
                continue;
            }

            const result = await fn(args);
            messages.push({
                role: 'tool',
                tool_call_id: toolCall.id,
                content: JSON.stringify(result)
            });
        }

        // Let model answer naturally on follow-up (don't force tool_choice)
        const savedToolChoice = chatClient.settings.toolChoice;
        chatClient.settings.toolChoice = undefined;
        response = await chatClient.completeChat(messages, tools);
        chatClient.settings.toolChoice = savedToolChoice;
        choice = response.choices[0]?.message;
    }

    return choice?.content ?? '';
}

// --- Main application ---
const manager = FoundryLocalManager.create({
    appName: 'learn_doc_assistant',
    logLevel: 'info'
});

let currentEp = '';
await manager.downloadAndRegisterEps((epName, percent) => {
    if (epName !== currentEp) {
        if (currentEp !== '') process.stdout.write('\n');
        currentEp = epName;
    }
    process.stdout.write(`\r  ${epName.padEnd(30)}  ${percent.toFixed(1).padStart(5)}%`);
});
if (currentEp !== '') process.stdout.write('\n');

const model = await manager.catalog.getModel('phi-4-mini');

await model.download((progress) => {
    process.stdout.write(`\rDownloading model: ${progress.toFixed(2)}%`);
});
console.log('\nModel downloaded.');

await model.load();
console.log('Model loaded and ready.');

const chatClient = model.createChatClient();
chatClient.settings.toolChoice = { type: 'required' };

const messages = [
    {
        role: 'system',
        content:
            'You are a Microsoft Learn documentation assistant. ' +
            'You MUST ALWAYS call the search_docs tool before answering ANY question. ' +
            'NEVER answer from your own knowledge. ' +
            'If the user asks about any Microsoft product, service, or technology, call search_docs first. ' +
            'Base your answer ONLY on the documentation returned by the tool. ' +
            'Include source URLs when available.'
    }
];

const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout
});

const askQuestion = (prompt) =>
    new Promise((resolve) => rl.question(prompt, resolve));

console.log(
    '\nLearn Doc Assistant ready! Ask about any Microsoft product or technology.'
);
console.log('Type \'quit\' to exit.\n');

while (true) {
    const userInput = await askQuestion('You: ');
    if (
        userInput.trim().toLowerCase() === 'quit' ||
        userInput.trim().toLowerCase() === 'exit'
    ) {
        break;
    }

    messages.push({ role: 'user', content: userInput });

    const response = await chatClient.completeChat(messages, tools);
    const answer = await processToolCalls(messages, response, chatClient);

    messages.push({ role: 'assistant', content: answer });
    console.log(`\nAssistant: ${answer}\n`);
}

await model.unload();
console.log('Model unloaded. Goodbye!');
rl.close();
