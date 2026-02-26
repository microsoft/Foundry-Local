// -------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
// -------------------------------------------------------------------------

import { FoundryLocalManager } from '../src/index.js';

/**
 * Helper function to multiply two numbers
 */
function multiplyNumbers(first: number, second: number): number {
    return first * second;
}

async function main() {
    let modelToLoad: any = null;

    try {
        // Initialize the Foundry Local SDK
        console.log('Initializing Foundry Local SDK...');
        
        // NOTE: You must update libraryPath to point to your built DLL if not in standard location
        const manager = FoundryLocalManager.create({
            appName: 'FoundryLocalExample',
            serviceEndpoint: 'http://localhost:5000',
            logLevel: 'info'
        });
        console.log('✓ SDK initialized successfully');

        // Explore available models
        console.log('\nFetching available models...');
        const catalog = manager.catalog;
        const models = await catalog.getModels();
        
        console.log(`Found ${models.length} models:`);
        for (const model of models) {
            const variants = model.variants.map((v: any) => v.id).join(', ');
            console.log(`  - ${model.alias} (variants: ${variants})`);
        }

        // Explore cached models
        console.log('\nFetching cached models...');
        const cachedModels = await catalog.getCachedModels();
        console.log(`Found ${cachedModels.length} cached models:`);
        for (const cachedModel of cachedModels) {
            console.log(`  - ${cachedModel.alias}`);
        }

        const modelAlias = 'qwen2.5-0.5b';
        
        // Load the model first
        console.log(`\nLoading model ${modelAlias}...`);
        modelToLoad = await catalog.getModel(modelAlias);
        if (!modelToLoad) {
            throw new Error(`Model ${modelAlias} not found`);
        }

        await modelToLoad.download();
        await modelToLoad.load();
        console.log('✓ Model loaded');

        // Create chat client
        console.log('\nCreating chat client...');
        const chatClient = modelToLoad.createChatClient();
        
        // Configure chat settings
        chatClient.settings.temperature = 0.7;
        chatClient.settings.maxTokens = 800;
        
        console.log('✓ Chat client created');

        // Prepare messages
        const messages = [
            { role: 'system', content: 'You are a helpful AI assistant. If necessary, you can use any provided tools to answer the question.' },
            { role: 'user', content: 'What is the answer to 7 multiplied by 6?' }
        ];

        // Prepare tools
        const tools = [
            {
                type: 'function',
                function: {
                    name: 'multiply_numbers',
                    description: 'A tool for multiplying two numbers.',
                    parameters: {
                        type: 'object',
                        properties: {
                            first: {
                                type: 'integer',
                                description: 'The first number in the operation'
                            },
                            second: {
                                type: 'integer',
                                description: 'The second number in the operation'
                            }
                        },
                        required: ['first', 'second']
                    }
                }
            }
        ];

        // First turn: Force the model to call the tool
        console.log('\nTesting chat completion with required tool use...');
        chatClient.settings.toolChoice = {
            type: 'required'  // Force the model to make a tool call
        };

        let toolCallData: any = null;
        console.log('Chat completion response:');
        
        await chatClient.completeStreamingChat(
            messages,
            tools,
            (chunk: any) => {
                const content = chunk.choices?.[0]?.message?.content;
                if (content) {
                    process.stdout.write(content);
                }
                
                // Capture tool call data
                const toolCalls = chunk.choices?.[0]?.message?.tool_calls;
                if (toolCalls && toolCalls.length > 0) {
                    toolCallData = toolCalls[0];
                }
            }
        );
        console.log('\n');

        // Handle tool invocation
        if (toolCallData && toolCallData.function?.name === 'multiply_numbers') {
            // Parse the arguments
            const argsJson = JSON.parse(toolCallData.function?.arguments || '{}');
            const first = argsJson.first;
            const second = argsJson.second;

            console.log(`\nInvoking tool: ${toolCallData.function?.name} with arguments ${first} and ${second}`);
            
            const result = multiplyNumbers(first, second);
            console.log(`Tool response: ${result}`);
            
            // Append the tool response to messages
            messages.push({
                role: 'tool',
                content: result.toString()
            });
        }

        // Second turn: Switch to auto tool choice and continue the conversation
        console.log('\nTool calls completed. Prompting model to continue conversation...\n');
        
        messages.push({
            role: 'system',
            content: 'Respond only with the answer generated by the tool.'
        });

        chatClient.settings.toolChoice = {
            type: 'auto'  // Let the model decide whether to use tools
        };

        console.log('Chat completion response:');
        await chatClient.completeStreamingChat(
            messages,
            tools,
            (chunk: any) => {
                const content = chunk.choices?.[0]?.message?.content;
                if (content) {
                    process.stdout.write(content);
                }
            }
        );
        console.log('\n');

        console.log('\n✓ Example completed successfully');

    } catch (error) {
        console.log('Error running example:', error);
        // Print stack trace if available
        if (error instanceof Error && error.stack) {
            console.log(error.stack);
        }
        process.exitCode = 1;
    } finally {
        if (modelToLoad) {
            try {
                if (await modelToLoad.isLoaded()) {
                    console.log('\nUnloading model...');
                    await modelToLoad.unload();
                    console.log('✓ Model unloaded');
                }
            } catch (cleanupError) {
                console.warn('Cleanup warning during model unload:', cleanupError);
            }
        }
    }
}

// Run the example
main().catch(console.error);

export { main };
