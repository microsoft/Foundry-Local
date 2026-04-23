// -------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
// -------------------------------------------------------------------------

import * as fs from 'fs';
import { FoundryLocalManager, getOutputText, createImageContentFromFile } from '../src/index.js';
import type { StreamingEvent, FunctionToolDefinition, FunctionCallItem, MessageItem } from '../src/types.js';

async function main() {
    try {
        // Initialize the Foundry Local SDK
        console.log('Initializing Foundry Local SDK...');
        const manager = FoundryLocalManager.create({
            appName: 'ResponsesExample',
            logLevel: 'info'
        });
        console.log('✓ SDK initialized');

        // Load a model
        const modelAlias = 'MODEL_ALIAS'; // Replace with a valid model alias
        const catalog = manager.catalog;
        const model = await catalog.getModel(modelAlias);
        await model.load();
        console.log(`✓ Model ${model.id} loaded`);

        // Start the web service (required for Responses API)
        manager.startWebService();
        console.log(`✓ Web service running at ${manager.urls[0]}`);

        // Create a ResponsesClient
        const client = manager.createResponsesClient(model.id);
        client.settings.temperature = 0.7;
        client.settings.maxOutputTokens = 500;

        // =================================================================
        // Example 1: Basic text response
        // =================================================================
        console.log('\n--- Example 1: Basic text response ---');
        const response = await client.create('What is the capital of France?');

        console.log(`Status: ${response.status}`);
        console.log(`Response: ${getOutputText(response)}`);

        // =================================================================
        // Example 2: Streaming response
        // =================================================================
        console.log('\n--- Example 2: Streaming response ---');
        process.stdout.write('Response: ');
        await client.createStreaming(
            'Write a short haiku about code.',
            (event: StreamingEvent) => {
                if (event.type === 'response.output_text.delta') {
                    process.stdout.write(event.delta);
                }
            }
        );
        console.log('\n');

        // =================================================================
        // Example 3: Multi-turn with previous_response_id
        // =================================================================
        console.log('--- Example 3: Multi-turn conversation ---');
        client.settings.store = true;

        const turn1 = await client.create('My name is Alice. Remember it.');
        console.log(`Turn 1 (ID: ${turn1.id}): done`);

        const turn2 = await client.create('What is my name?', {
            previous_response_id: turn1.id,
        });
        console.log(`Turn 2: ${getOutputText(turn2)}`);

        // =================================================================
        // Example 4: Tool calling
        // =================================================================
        console.log('\n--- Example 4: Tool calling ---');
        const tools: FunctionToolDefinition[] = [{
            type: 'function',
            name: 'get_weather',
            description: 'Get the current weather for a location.',
            parameters: {
                type: 'object',
                properties: {
                    location: { type: 'string', description: 'City name' },
                },
                required: ['location'],
            },
        }];

        const toolResponse = await client.create(
            'What is the weather in Seattle?',
            { tools, tool_choice: 'required' }
        );

        // Find the function call in the output
        const funcCall = toolResponse.output.find(
            (o): o is FunctionCallItem => o.type === 'function_call'
        );

        if (funcCall) {
            console.log(`Tool call: ${funcCall.name}(${funcCall.arguments})`);

            // Simulate providing the tool result and continuing
            const finalResponse = await client.create([
                { type: 'function_call_output', call_id: funcCall.call_id, output: '72°F, sunny' },
            ], { previous_response_id: toolResponse.id, tools });

            console.log(`Final: ${getOutputText(finalResponse)}`);
        }

        // =================================================================
        // Example 5: Get & delete stored response
        // =================================================================
        console.log('\n--- Example 5: Get & delete stored response ---');
        const stored = await client.create('Hello!');
        console.log(`Created: ${stored.id}`);

        const retrieved = await client.get(stored.id);
        console.log(`Retrieved: ${retrieved.id}, status: ${retrieved.status}`);

        const deleted = await client.delete(stored.id);
        console.log(`Deleted: ${deleted.deleted}`);

        // =================================================================
        // Example 6: List all stored responses
        // =================================================================
        console.log('\n--- Example 6: List stored responses ---');
        const allResponses = await client.list();
        console.log(`Listed ${allResponses.data.length} stored responses`);

        // =================================================================
        // Example 7: Vision — describe an image
        // =================================================================
        console.log('\n--- Example 7: Vision ---');
        const testImagePath = 'path/to/test-image.png'; // Replace with a real image path
        if (fs.existsSync(testImagePath)) {
            const imageContent = createImageContentFromFile(testImagePath);
            const visionResponse = await client.create([
                {
                    type: 'message',
                    role: 'user',
                    content: [
                        { type: 'input_text', text: 'Describe this image in one sentence.' },
                        imageContent,
                    ],
                } as MessageItem,
            ]);
            console.log(`Vision: ${getOutputText(visionResponse)}`);
        } else {
            console.log('(Skipped: test image not found)');
        }

        // Cleanup
        manager.stopWebService();
        await model.unload();
        console.log('\n✓ Example completed successfully');

    } catch (error) {
        console.error('Error:', error instanceof Error ? error.message : error);
        process.exit(1);
    }
}

main();
