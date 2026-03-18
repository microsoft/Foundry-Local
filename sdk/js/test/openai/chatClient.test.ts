import { describe, it } from 'mocha';
import { expect } from 'chai';
import { getMultiplyTool, getTestManager, TEST_MODEL_ALIAS } from '../testUtils.js';

describe('Chat Client Tests', () => {
    it('should perform chat completion', async function() {
        this.timeout(10000);
        const manager = getTestManager();
        const catalog = manager.catalog;
        
        // Ensure cache is populated first
        const cachedModels = await catalog.getCachedModels();
        expect(cachedModels.length).to.be.greaterThan(0);

        const cachedVariant = cachedModels.find(m => m.alias === TEST_MODEL_ALIAS);
        expect(cachedVariant).to.not.be.undefined;

        const model = await catalog.getModel(TEST_MODEL_ALIAS);
        
        expect(model).to.not.be.undefined;
        if (!cachedVariant) return;

        model.selectVariant(cachedVariant.id);

        await model.load();
        
        try {
            const client = model.createChatClient();

            client.settings.maxTokens = 500;
            client.settings.temperature = 0.0; // for deterministic results

            const result = await client.completeChat([
                { role: 'user', content: 'You are a calculator. Be precise. What is the answer to 7 multiplied by 6?' }
            ]);
            
            expect(result).to.not.be.undefined;
            expect(result.choices).to.be.an('array');
            expect(result.choices.length).to.be.greaterThan(0);
            expect(result.choices[0].message).to.not.be.undefined;
            expect(result.choices[0].message.content).to.be.a('string');
            console.log(`Response: ${result.choices[0].message.content}`);
            expect(result.choices[0].message.content).to.include('42');
        } finally {
            await model.unload();
        }
    });

    it('should perform streaming chat completion', async function() {
        this.timeout(10000);
        const manager = getTestManager();
        const catalog = manager.catalog;
        
        // Ensure cache is populated first
        const cachedModels = await catalog.getCachedModels();
        expect(cachedModels.length).to.be.greaterThan(0);

        const cachedVariant = cachedModels.find(m => m.alias === TEST_MODEL_ALIAS);
        expect(cachedVariant).to.not.be.undefined;

        const model = await catalog.getModel(TEST_MODEL_ALIAS);
        
        expect(model).to.not.be.undefined;
        if (!cachedVariant) return;

        model.selectVariant(cachedVariant.id);

        await model.load();
        
        try {
            const client = model.createChatClient();

            client.settings.maxTokens = 500;
            client.settings.temperature = 0.0; // for deterministic results

            const messages = [
                { role: 'user', content: 'You are a calculator. Be precise. What is the answer to 7 multiplied by 6?' }
            ];

            // First question
            let fullContent = '';
            let chunkCount = 0;

            await client.completeStreamingChat(messages, (chunk: any) => {
                chunkCount++;
                const content = chunk.choices?.[0]?.delta?.content;
                if (content) {
                    fullContent += content;
                }
            });
            
            expect(chunkCount).to.be.greaterThan(0);
            expect(fullContent).to.be.a('string');
            console.log(`First response: ${fullContent}`);
            expect(fullContent).to.include('42');

            // Add assistant's response and ask follow-up question
            messages.push({ role: 'assistant', content: fullContent });
            messages.push({ role: 'user', content: 'Add 25 to the previous answer. Think hard to be sure of the answer.' });

            // Second question
            fullContent = '';
            chunkCount = 0;

            await client.completeStreamingChat(messages, (chunk: any) => {
                chunkCount++;
                const content = chunk.choices?.[0]?.delta?.content;
                if (content) {
                    fullContent += content;
                }
            });

            expect(chunkCount).to.be.greaterThan(0);
            expect(fullContent).to.be.a('string');
            console.log(`Second response: ${fullContent}`);
            expect(fullContent).to.include('67');
        } finally {
            await model.unload();
        }
    });

    it('should throw when completing chat with empty, null, or undefined messages', async function() {
        const manager = getTestManager();
        const catalog = manager.catalog;
        const model = await catalog.getModel(TEST_MODEL_ALIAS);

        const client = model.createChatClient();
        
        const invalidMessages: any[] = [[], null, undefined];
        for (const invalidMessage of invalidMessages) {
            try {
                await client.completeChat(invalidMessage);
                expect.fail(`Should have thrown an error for ${Array.isArray(invalidMessage) ? 'empty' : invalidMessage} messages`);
            } catch (error) {
                expect(error).to.be.instanceOf(Error);
                expect((error as Error).message).to.include('Messages array cannot be null, undefined, or empty.');
            }
        }
    });

    it('should throw when completing chat with invalid message', async function() {
        const manager = getTestManager();
        const catalog = manager.catalog;
        const model = await catalog.getModel(TEST_MODEL_ALIAS);

        const client = model.createChatClient();
        
        try {
            await client.completeChat([{ role: 'user' } as any]);
            expect.fail('Should have thrown an error for message without content');
        } catch (error) {
            expect(error).to.be.instanceOf(Error);
            expect((error as Error).message).to.include('Each message must have a "content" property that is a non-empty string.');
        }

        try {
            await client.completeChat([{ content: 'hello' } as any]);
            expect.fail('Should have thrown an error for message without role');
        } catch (error) {
            expect(error).to.be.instanceOf(Error);
            expect((error as Error).message).to.include('Each message must have a "role" property that is a non-empty string.');
        }
    });

    it('should throw when completing streaming chat with empty, null, or undefined messages', async function() {
        const manager = getTestManager();
        const catalog = manager.catalog;
        const model = await catalog.getModel(TEST_MODEL_ALIAS);

        const client = model.createChatClient();
        
        const invalidMessages: any[] = [[], null, undefined];
        for (const invalidMessage of invalidMessages) {
            try {
                await client.completeStreamingChat(invalidMessage, () => {});
                expect.fail(`Should have thrown an error for ${Array.isArray(invalidMessage) ? 'empty' : invalidMessage} messages`);
            } catch (error) {
                expect(error).to.be.instanceOf(Error);
                expect((error as Error).message).to.include('Messages array cannot be null, undefined, or empty.');
            }
        }
    });

    it('should throw when completing streaming chat with invalid callback', async function() {
        const manager = getTestManager();
        const catalog = manager.catalog;
        const model = await catalog.getModel(TEST_MODEL_ALIAS);
        const client = model.createChatClient();
        const messages = [{ role: 'user', content: 'Hello' }];
        const invalidCallbacks: any[] = [null, undefined, {} as any, 'not a function' as any];
        for (const invalidCallback of invalidCallbacks) {
            try {
                await client.completeStreamingChat(messages as any, invalidCallback as any);
                expect.fail('Should have thrown an error for invalid callback');
            } catch (error) {
                expect(error).to.be.instanceOf(Error);
            }
        }
    });

    it('should perform tool calling chat completion (non-streaming)', async function() {
        this.timeout(20000);
        const manager = getTestManager();
        const catalog = manager.catalog;

        const cachedModels = await catalog.getCachedModels();
        expect(cachedModels.length).to.be.greaterThan(0);

        const cachedVariant = cachedModels.find(m => m.alias === TEST_MODEL_ALIAS);
        expect(cachedVariant).to.not.be.undefined;

        const model = await catalog.getModel(TEST_MODEL_ALIAS);
        expect(model).to.not.be.undefined;
        if (!cachedVariant) return;

        model.selectVariant(cachedVariant.id);
        await model.load();

        try {
            const client = model.createChatClient();
            client.settings.maxTokens = 500;
            client.settings.temperature = 0.0;
            client.settings.toolChoice = { type: 'required' }; // Force the model to make a tool call

            // Prepare messages and tools
            const messages: any[] = [
                { role: 'system', content: 'You are a helpful AI assistant. If necessary, you can use any provided tools to answer the question.' },
                { role: 'user', content: 'What is the answer to 7 multiplied by 6?' }
            ];
            const tools: any[] = [getMultiplyTool()];

            // Start the conversation
            let response = await client.completeChat(messages, tools);

            // Check that a tool call was generated
            expect(response).to.not.be.undefined;
            expect(response.choices).to.be.an('array').with.length.greaterThan(0);
            expect(response.choices[0].finish_reason).to.equal('tool_calls');
            expect(response.choices[0].message).to.not.be.undefined;
            expect(response.choices[0].message.tool_calls).to.be.an('array').with.length.greaterThan(0);

            // Check the tool call generated by the model
            const toolCall = response.choices[0].message.tool_calls[0];
            expect(toolCall.type).to.equal('function');
            expect(toolCall.function?.name).to.equal('multiply_numbers');

            const args = JSON.parse(toolCall.function?.arguments ?? '{}');
            expect(args.first).to.equal(7);
            expect(args.second).to.equal(6);

            // Add the response from invoking the tool call to the conversation and check if the model can continue correctly
            messages.push({ role: 'tool', content: '7 x 6 = 42.' });

            // Prompt the model to continue the conversation after the tool call
            messages.push({ role: 'system', content: 'Respond only with the answer generated by the tool.' });

            // Set tool calling back to auto so that the model can decide whether to call
            // the tool again or continue the conversation based on the new user prompt
            client.settings.toolChoice = { type: 'auto' };

            // Run the next turn of the conversation
            response = await client.completeChat(messages, tools);

            // Check that the conversation continued
            expect(response.choices[0].message.content).to.be.a('string');
            expect(response.choices[0].message.content).to.include('42');
        } finally {
            await model.unload();
        }
    });

    it('should perform tool calling chat completion (streaming)', async function() {
        this.timeout(30000);
        const manager = getTestManager();
        const catalog = manager.catalog;

        const cachedModels = await catalog.getCachedModels();
        expect(cachedModels.length).to.be.greaterThan(0);

        const cachedVariant = cachedModels.find(m => m.alias === TEST_MODEL_ALIAS);
        expect(cachedVariant).to.not.be.undefined;

        const model = await catalog.getModel(TEST_MODEL_ALIAS);
        expect(model).to.not.be.undefined;
        if (!cachedVariant) return;

        model.selectVariant(cachedVariant.id);
        await model.load();

        try {
            const client = model.createChatClient();
            client.settings.maxTokens = 500;
            client.settings.temperature = 0.0;
            client.settings.toolChoice = { type: 'required' }; // Force the model to make a tool call

            // Prepare messages and tools
            const messages: any[] = [
                { role: 'system', content: 'You are a helpful AI assistant. If necessary, you can use any provided tools to answer the question.' },
                { role: 'user', content: 'What is the answer to 7 multiplied by 6?' }
            ];
            const tools: any[] = [getMultiplyTool()];

            // Start the conversation
            let fullResponse = '';
            let lastToolCallChunk: any = null;

            // Check that each response chunk contains the expected information
            await client.completeStreamingChat(messages, tools, (chunk: any) => {
                const content = chunk.choices?.[0]?.message?.content ?? chunk.choices?.[0]?.delta?.content;
                if (content) {
                    fullResponse += content;
                }
                const toolCalls = chunk.choices?.[0]?.message?.tool_calls;
                if (toolCalls && toolCalls.length > 0) {
                    lastToolCallChunk = chunk;
                }
            });

            expect(fullResponse).to.be.a('string').and.not.equal('');
            expect(lastToolCallChunk).to.not.be.null;

            // Check that the full response contains the expected tool call and that the tool call information is correct
            const toolCall = lastToolCallChunk.choices[0].message.tool_calls[0];
            expect(lastToolCallChunk.choices[0].finish_reason).to.equal('tool_calls');
            expect(toolCall.type).to.equal('function');
            expect(toolCall.function?.name).to.equal('multiply_numbers');

            const args = JSON.parse(toolCall.function?.arguments ?? '{}');
            expect(args.first).to.equal(7);
            expect(args.second).to.equal(6);

            // Add the response from invoking the tool call to the conversation and check if the model can continue correctly
            messages.push({ role: 'tool', content: '7 x 6 = 42.' });

            // Prompt the model to continue the conversation after the tool call
            messages.push({ role: 'system', content: 'Respond only with the answer generated by the tool.' });

            // Set tool calling back to auto so that the model can decide whether to call
            // the tool again or continue the conversation based on the new user prompt
            client.settings.toolChoice = { type: 'auto' };

            // Run the next turn of the conversation
            fullResponse = '';
            await client.completeStreamingChat(messages, tools, (chunk: any) => {
                const content = chunk.choices?.[0]?.message?.content ?? chunk.choices?.[0]?.delta?.content;
                if (content) {
                    fullResponse += content;
                }
            });

            // Check that the conversation continued
            expect(fullResponse).to.be.a('string').and.not.equal('');
            expect(fullResponse).to.include('42');
        } finally {
            await model.unload();
        }
    });
});
